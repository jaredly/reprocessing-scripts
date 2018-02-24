
open Infix;

/**
 * TODO TODO
 *
 * in order to fully support bucklescript libraries...
 *
 * - support the `ppx` thing
 * - recursive dependencies
 * - maybe bsc-flags & warnings?
 *
 * also, I'll want a way to specify dependencies that are for a given target.
 */

let rec getSubdirs = dir => {
  ReasonCliTools.Files.readDirectory(dir) |> List.fold_left((directories, item) => {
    let full = Filename.concat(dir, item);
    if (ReasonCliTools.Files.isDirectory(full)) {
      [full, ...getSubdirs(full)] @ directories
    } else {
      directories
    }
  }, [])
};

let gatherSourceDirectories = (baseDirectory, bsconfig) => {
  Json.get("sources", bsconfig)
  |?> Json.array |? []
  |> List.fold_left((directories, item) => {
    switch item {
    | Json.String(contents) => [Filename.concat(baseDirectory, contents), ...directories]
    | Json.Object(_) => {
      let dir = Json.get("dir", item) |?> Json.string |! "Source must have a 'dir' that is a string";
      let dir = Filename.concat(baseDirectory, dir);
      let subdirs = Json.get("subdirs", item) |?> Json.bool |? false;
      if (subdirs) {
        [dir, ...getSubdirs(dir)] @ directories
      } else {
        /* TODO children and such */
        [dir, ...directories]
      }
    }
    | _ => failwith("Bad source")
    }
  }, [])
};

let convertToReason = (refmt, fileName) => {
  let (out, success) = ReasonCliTools.Commands.execSync(~cmd=refmt ++ " --print ml --parse re " ++ fileName, ());
  if (!success) {
    failwith("refmt died");
  } else {
    String.concat("\n", out)
  }
};

let capitalize = name => String.uppercase(String.sub(name, 0, 1)) ++ String.sub(name, 1, String.length(name) - 1);

/** TODO replace invalid characters n such */
let moduleName = name => {
  let name = Filename.basename(name) |> Filename.chop_extension;
  capitalize(name)
};

let stripNonModuleThings = name => Str.global_replace(Str.regexp("[^a-zA-Z0-9_]"), "", name);

let packageLibraryContents = (baseDirectory, bsconfig, ~ocamlDir, ~refmt, ~ppx, ~env) => {
  let sourceDirectories = gatherSourceDirectories(baseDirectory, bsconfig);
  let sourceFiles = List.concat(List.map(d => ReasonCliTools.Files.readDirectory(d) |> List.map(Filename.concat(d)), sourceDirectories))
  |> List.filter(Utils.isSourceFile)
  |> List.filter(ReasonCliTools.Files.isFile);
  let depsList = Getdeps.runOcamlDep(~sourceFiles, ~sourceDirectories, ~ocamlDir, ~refmt, ~ppx, ~env) |! "Unable to run ocamldep";
  let sorted = Getdeps.sortDeps(depsList);

  List.map(name => {
    let contents = Filename.check_suffix(name, ".ml") ? convertToReason(refmt, name) : ReasonCliTools.Files.readFile(name) |! "Could not read source file";
    "/** " ++ name ++ " **/\nmodule " ++ moduleName(name) ++ " = {\n" ++ contents ++ "\n};"
  }, sorted)
  |> String.concat("\n\n")
};
