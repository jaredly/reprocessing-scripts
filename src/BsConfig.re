
open Infix;

  /* "bs-dependencies": ["@jaredly/reprocessing", "maze.re"], */

let processDeps = bsconfig => {
  let rootDirectory = "./";
  let items = Json.get("bs-dependencies", bsconfig) |?> Json.array |?>> List.map(Json.string |.! "dep must be a string") |? []
  /* I do my own thing */
  |> List.filter(name => name != "@jaredly/reprocessing")
  ;
  let directories = items |> List.map(name => BuildUtils.findNodeModule(name, "./node_modules") |! "unable to find required dependency");
  let withConfig = directories |> List.map(directory => (directory, ReasonCliTools.Files.readFile(Filename.concat(directory, "bsconfig.json")) |! "unable to read bsconfig.json" |> Json.parse));
  List.fold_left(((packagedLibs, sourceDirectories), (directory, bsconfig)) => {
    /** TODO TODO recursive dependencies */
    let namespace = Json.get("namespace", bsconfig) |?> Json.bool |? false;
    if (namespace) {
      ([(directory, bsconfig), ...packagedLibs], sourceDirectories)
    } else {
      (packagedLibs, [directory, ...sourceDirectories])
    }
  },
  ([], []),
  withConfig
  )
};
