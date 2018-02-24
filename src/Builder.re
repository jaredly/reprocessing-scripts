
let isNewer = (src, dest) => {
  let stat = try (Some(Unix.stat(dest))) {
  | Unix.Unix_error(Unix.ENOENT, _, _) => None
  };
  switch stat {
  | None => true
  | Some(stat) => {
    let ss = Unix.stat(src);
    ss.Unix.st_mtime > stat.Unix.st_mtime
  }
  }
};

let copyIfNewer = (src, dest) => {
  if (isNewer(src, dest)) {
    BuildUtils.copy(src, dest)
  }
};

let copyDirContents = (source, dest) => {
  let contents = BuildUtils.readdir(source)
  |> List.filter(x => x != "." && x != "..")
  |> List.filter(name => {
      let src = Filename.concat(source, name);
      let stat = Unix.stat(src);
      stat.Unix.st_kind === Unix.S_REG
  });
  List.map(
    name => {
      /* print_endline(name); */
      copyIfNewer(
        Filename.concat(source, name),
        Filename.concat(dest, name)
      );
      Filename.concat(dest, name)
    },
    contents
  )
};

let unwrap = (message, opt) => switch opt { | None => failwith(message) | Some(x) => x};

type config = {
  name: string,
  byte: bool,
  env: string,
  mainFile: string,
  cOpts: string,
  mlOpts: string,
  dependencyDirs: list(string),
  packagedLibs: list((string, Json.t)),
  shared: bool,
  outDir: string,
  buildDir: string,
  ocamlDir: string,
  cc: string,
  refmt: string,
  ppx: list(string),
};

let copyAndSort = ({env, mainFile, dependencyDirs, packagedLibs, buildDir, ocamlDir, refmt, ppx}) => {
  try (Unix.stat(buildDir) |> ignore) {
  | Unix.Unix_error(Unix.ENOENT, _, _) => Unix.mkdir(buildDir, 0o740);
  };
  let allNames = List.map(dirname => copyDirContents(dirname, buildDir), [Filename.dirname(mainFile), ...dependencyDirs]) |> List.concat;

  let allNames = allNames @ List.map(
    ((directory, bsconfig)) => {
      open Infix;
      let contents = Namespace.packageLibraryContents(directory, bsconfig, ~ocamlDir, ~refmt, ~ppx, ~env);
      let packageName = Json.get("name", bsconfig) |?> Json.string |! "package name must be a string";
      let moduleName = Namespace.stripNonModuleThings(packageName) |> Namespace.capitalize;
      let dest = Filename.concat(buildDir, moduleName ++ ".re");
      ReasonCliTools.Files.writeFile(dest, contents);
      dest
    },
    packagedLibs
  );

  let mainFileName = Filename.concat(buildDir, Filename.basename(mainFile));
  let reasonOrOcamlFiles = List.filter(Utils.isSourceFile, allNames);
  let filesInOrder = unwrap("Failed to run ocamldep", Getdeps.sortSourceFilesInDependencyOrder(~ocamlDir, ~refmt, ~ppx, ~env, reasonOrOcamlFiles, mainFileName));
  (allNames, filesInOrder)
};

let ocamlopt = config => {
  let ppxFlags = String.concat(" ", List.map(name => "-ppx " ++ name, config.ppx));
  Printf.sprintf(
    "%s %s %s %s %s %s -I %s -w -40 -pp '%s --print binary'", /*  -verbose */
    config.env,
    Filename.concat(config.ocamlDir, "bin/ocamlrun"),
    Filename.concat(config.ocamlDir, config.byte ? "bin/ocamlc" : "bin/ocamlopt"),
    ppxFlags,
    Str.split(Str.regexp(" "), config.cOpts) |> List.map(x => "-ccopt " ++ x) |> String.concat(" "),
    config.mlOpts,
    Filename.concat(config.ocamlDir, "lib/ocaml"),
    config.refmt
  )
};

let exists = path => try {Unix.stat(path) |> ignore; true} {
| Unix.Unix_error(Unix.ENOENT, _, _) => false
};

let compileMl = (config, force, sourcePath) => {
  let cmx = Filename.chop_extension(sourcePath) ++ (config.byte ? ".cmo" : ".cmx");
  if (force || isNewer(sourcePath, cmx)) {
    BuildUtils.readCommand(Printf.sprintf(
      "%s -c -thread %s -I %s -o %s -impl %s",
      ocamlopt(config),
      config.byte ? "" : "-S",
      Filename.dirname(sourcePath),
      cmx,
      sourcePath
    )) |> unwrap(
      "Failed to build " ++ sourcePath
    ) |> ignore;
    (true, cmx)
  } else {
    (false, cmx)
  }
};

let compileC = (config, force, sourcePath) => {
  let dest = Filename.chop_extension(sourcePath) ++ ".o";
  if (force || isNewer(sourcePath, dest)) {
    /* let out = Filename.basename(dest); */
    BuildUtils.readCommand(Printf.sprintf(
      "%s %s -I %s -c -std=c11 %s -o %s",
      config.cc,
      config.cOpts,
      /* ocamlopt(config), */
      Filename.dirname(sourcePath),
      sourcePath,
      dest
    )) |> unwrap(
      "Failed to build " ++ sourcePath
    ) |> ignore;
    /* BuildUtils.copy(out, dest); */
    /* Unix.unlink(out); */
    (true, dest)
  } else {
    (false, dest)
  };
};

open ReasonCliTools;
let compileShared = (config, cmxs, os) => {
  let dest = Filename.concat(config.outDir, "lib" ++ config.name ++ ".so");
  let initial = "lib" ++ config.name ++ ".so";

  let ext = config.byte ? ".cma" : ".cmxa";
  let sourceFiles = [
    /* Filename.concat(config.ocamlDir, "lib/ocaml/libasmrun.a"), */
    "bigarray" ++ ext,
    "unix" ++ ext,
    "threads/threads" ++ ext,
    ...List.append(cmxs, os)
  ];
  let sourceFiles = config.byte ? ["dynlink.cma", ...sourceFiles] : sourceFiles;

  BuildUtils.readCommand(Printf.sprintf(
    "%s -I %s -output-obj %s -o %s",
    ocamlopt(config),
    config.buildDir,
    String.concat(" ", sourceFiles),
    initial
  )) |> unwrap(
    "Failed to build " ++ initial
  ) |> ignore;
  Files.copy(~source=initial, ~dest=dest) |> ignore;
  Unix.unlink(initial);
  let cds = "lib" ++ config.name ++ ".cds";
  if (Files.exists(cds)) {
    Unix.unlink(cds);
  };
};

let compileStatic = (config, cmxs, os) => {
  let ofile = Filename.concat(config.buildDir, "libcustom" ++ ".o");
  let dest = Filename.concat(config.outDir, "lib" ++ config.name ++ ".a");
  let ext = config.byte ? ".cma" : ".cmxa";
  let sourceFiles = [
    Filename.concat(config.ocamlDir, "lib/ocaml/libasmrun.a"),
    "bigarray" ++ ext,
    /* "unix" ++ ext, */
    "threads/threads" ++ ext,
    ...List.append(cmxs, os)
  ];
  /* let sourceFiles = config.byte ? ["dynlink.cma", ...sourceFiles] : sourceFiles; */
  BuildUtils.readCommand(Printf.sprintf(
    "%s -I %s -I %s -cclib -lasmrun -ccopt -lasmrun -cclib -static -output-obj %s -o %s",
    ocamlopt(config),
    config.buildDir,
    config.ocamlDir,
    String.concat(" ", sourceFiles),
    "libcustom.o"
  )) |> unwrap(
    "Failed to build " ++ dest
  ) |> ignore;
  BuildUtils.copy("libcustom.o", ofile);
  Unix.unlink("libcustom.o");
  BuildUtils.copy(
    Filename.concat(config.ocamlDir, "lib/ocaml/libasmrun.a"),
    dest
  );
  let staticDir = Filename.concat(config.buildDir, "static_as");
  BuildUtils.mkdirp(staticDir);
  /* BuildUtils.readCommand("cd static_as && ar -x " ++ config.ocamlDir ++ "/lib/ocaml/libunix.a")
  |> unwrap("failed to get unix stuff") |> ignore; */
  BuildUtils.readCommand("cd " ++ staticDir ++ " && ar -x " ++ config.ocamlDir ++ "/lib/ocaml/libbigarray.a")
  |> unwrap("failed to get unix stuff") |> ignore;
  BuildUtils.readCommand("cd " ++ staticDir ++ " && ar -x " ++ config.ocamlDir ++ "/lib/ocaml/libthreads.a")
  |> unwrap("failed to get unix stuff") |> ignore;
  let os = os @ (
    ReasonCliTools.Files.readDirectory(staticDir)
    |> List.filter(x => Filename.check_suffix(x, ".o"))
    |> List.map(Filename.concat(staticDir))
  );

  BuildUtils.readCommand(Printf.sprintf(
    "ar -r %s %s %s",
    dest,
    String.concat(" ", os),
    ofile
  )) |> unwrap("failed to link") |> ignore;

  ReasonCliTools.Files.removeDeep(staticDir);
};

let mapNewer = (fn, files) => {
  List.fold_left(
    ((force, results), name) => {
      let (changed, result) = fn(force, name);
      (changed || force, results @ [result])
    },
    (false, []),
    files
  ) |> snd
};

let compile = config => {
  print_endline("Building " ++ config.outDir ++ config.name);
  BuildUtils.mkdirp(config.outDir);
  BuildUtils.readCommand(ocamlopt(config) ++ " -config") |> unwrap(
    "OCaml compiler not set up correctly - can't run -config"
  ) |> ignore;
  /* |>  String.concat("\n") |> print_endline; */
  let (allNames, filesInOrder) = copyAndSort(config);
  /** Build .cmx's */
  let cmxs = mapNewer(compileMl(config), filesInOrder);
  /** Build .o's */
  let os = mapNewer(compileC(config),
    List.filter(name => Filename.check_suffix(name, ".c") || Filename.check_suffix(name, ".m"), allNames));
  /** Build them together */
  config.shared ? compileShared(config, cmxs, os) : compileStatic(config, cmxs, os);
  print_endline("Built!");
};