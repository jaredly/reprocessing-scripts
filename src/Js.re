
let basic_html = {|
<!DOCTYPE html>
<html>
  <head>
    <meta charset=utf8>
    <meta name="viewport" content="initial-scale=1, maximum-scale=1">
    <title>My Reprocessing Game</title>
    <style> html, body { margin: 0;padding:0; } </style>
  </head>
  <body>
    <script src="./assets/bundle.js" ></script>
|};

let ensurePublic = () => {
  if (!Builder.exists("public")) Unix.mkdir("public", 0o740);
  if (!Builder.exists("public/assets")) Unix.mkdir("public/assets", 0o740);
  if (!Builder.exists("public/index.html")) {
    let fd = Unix.openfile("public/index.html", [Unix.O_WRONLY, Unix.O_CREAT, Unix.O_TRUNC], 0o640);
    let chan = Unix.out_channel_of_descr(fd);
    output_string(chan, basic_html);
    flush(chan);
    Unix.close(fd);
  };
};

/* let webpackCommand = "webpack --entry ./lib/js/src/web.js \
    --resolve-alias Reprocessing=@jaredly/reprocessing \
    --resolve-alias ReasonglInterface=@jaredly/reasongl-interface \
    --resolve-alias ReasonglWeb=@jaredly/reasongl-web \
    --output-path ./public/assets \
    --output-filename bundle.js"; */

let bsbCommand = "bsb -make-world -backend js";

let buildBundle = () => {
  let output = Packre.Pack.process(~renames=[], "./lib/js/src/web.js");
  ReasonCliTools.Files.writeFile("./public/assets/bundle.js", output) |> ignore;
};

let build = () => {
  ensurePublic();
  print_endline(">> Running bsb");
  print_newline();
  BuildUtils.readCommand(bsbCommand) |> Builder.unwrap("Unable to run bsb") |>  String.concat("\n    ") |> (x) => print_endline("\n    " ++ x);
  print_newline();
  print_endline(">> Running pack");
  print_newline();
  try (buildBundle()) {
  | Failure(message) => failwith("unable to run pack.re: " ++ message)
  };
  if (Builder.exists("assets")) {
    print_newline();
    print_endline(">> Copying assets");
    BuildUtils.copyDeep("./assets", "./public/assets");
  };
  print_newline();
  print_endline(">> Js build done <<")
};

let sendHot = (client, script) => {
  print_endline("Sending hot");
  open ReasonSimpleServer.Basic;
  sendToSocket(client, formatResponse(okTop("application/javascript"), script));
  Unix.shutdown(client, Unix.SHUTDOWN_ALL);
};

let startsWith = (string, prefix) => String.length(prefix) < String.length(string) && String.sub(string, 0, String.length(prefix)) == prefix;
let sliceToEnd = (string, num) => String.sub(string, num, String.length(string) - num);



let hot = () => {
  ensurePublic();
  if (Builder.exists("assets")) {
    print_endline(">> Copying assets");
    BuildUtils.copyDeep("./assets", "./public/assets");
  };

  BuildUtils.readCommand(bsbCommand) |> Builder.unwrap("Unable to run bsb") |>  String.concat("\n    ") |> (x) => print_endline("\n    " ++ x);
  let initialBundle = Packre.Pack.process(~renames=[], "./lib/js/src/webhot.js");
  /* ReasonCliTools.Files.writeFile("./public/assets/bundle.js", output) |> ignore; */

  /* BuildUtils.showCommand("open http://localhost:3451") |> ignore; */
  print_endline("Static server on http://localhost:3451");

  /* let previous = ref(Packre.Pack.process(~externalEverything=true, ~renames=[], "./lib/js/src/web.js")); */

  let previous = Hashtbl.create(5);

  let parked = Hashtbl.create(5);

  /* let waitingToSend = ref(Some(previous^));
  let waitingSocket = ref(None); */

  let jsFile = reFile => Filename.concat("./lib/js", Filename.chop_extension(reFile) ++ ".js");

  let search = (string, needle) => switch (Str.search_forward(Str.regexp(needle), string, 0)) {
  | exception Not_found => false
  | _ => true
  };

  let bsb = ReasonCliTools.Commands.exec(
    ~cmd=bsbCommand ++ " -w",
    ~onOut=text => {
      print_endline(text);
      if (search(text, "Finish compiling")) {
        print_endline("Static server on http://localhost:3451");

        let dead = ref([]);

        Hashtbl.iter((id, (filePath, socket)) => {
          print_endline("Here" ++ filePath);
          let text = try (Some(Packre.Pack.process(~externalEverything=true, ~renames=[], jsFile("." ++ filePath)))) {
          | Failure(message) => {
            print_endline("Failed to build");
              None
            }
          };
          let prevCode = switch (Hashtbl.find(previous, filePath ++ id)) {
          | exception Not_found => ""
          | x => x
          };
          switch text {
          | None => print_endline("Pack.re bundling failed")
          | Some(text) when text == prevCode => {
            print_endline("Rebuilt but same " ++ filePath);
          }
          | Some(text) =>
            print_endline("Rebuilt the js bundle for " ++ filePath);
            Hashtbl.replace(previous, filePath ++ id, text);
            try (sendHot(socket, text)) {
            | _ => dead := [id, ...dead^]
            };
          };

        }, parked);
        List.iter(id => Hashtbl.remove(parked, id), dead^)
      } else {print_endline("<no match>")};

      ()
    }
  );

  open ReasonSimpleServer.Basic.Response;

  /* TODO if there are problems with multiple changes happening in succession and the
     hot client only getting the first one, then I'll want to go back to tracking
     pending stuff.... */
  ReasonSimpleServer.Static.run(
    ~extraHandler=(method, path, headers) => {
      switch (method, path) {
      | ("GET", path) when startsWith(path, "/hot-first/") =>
        let path = sliceToEnd(path, String.length("/hot-first"));
        switch (Str.split(Str.regexp("\\?"), path)) {
        | [filePath, id] => {
          if (ReasonCliTools.Files.exists("." ++ filePath)) {
            Some(Custom(socket => {
              let code = Packre.Pack.process(~externalEverything=true, ~renames=[], jsFile("." ++ filePath));
              Hashtbl.replace(previous, filePath ++ id, code);
              sendHot(socket, code);
            }));
          } else {
            Some(Bad(404, "No such resaon file exists"))
          }
        }
        | _ => Some(Bad(400, "Need a ?adfjl id suffix"))
        }
      | ("GET", path) when startsWith(path, "/hot/") =>
        let path = sliceToEnd(path, String.length("/hot"));
        switch (Str.split(Str.regexp("\\?"), path)) {
        | [filePath, id] => {
          if (ReasonCliTools.Files.exists("." ++ filePath)) {
            Some(Custom(socket => {
              Hashtbl.replace(parked, id, (filePath, socket))
            }));
          } else {
            Some(Bad(404, "No such resaon file exists"))
          }
        }
        | _ => Some(Bad(400, "Need a ?adfjl id suffix"))
        }
      | ("GET", "/assets/bundle.js") => {
        print_endline("Getting main bundle");
        Some(ReasonSimpleServer.Basic.Response.Ok("application/javascript", initialBundle))
      }
      | _ => {
        print_endline("Skip whatever " ++ path);
        None
      }
    }
    },
    ~poll=() => ReasonCliTools.Commands.poll(bsb),
    ~port=3451,
    "./public"
  )
};

/*
let watch = () => {
  ensurePublic();
  if (Builder.exists("assets")) {
    print_endline(">> Copying assets");
    BuildUtils.copyDeep("./assets", "./public/assets");
  };
  let buffers = ref([|"", ""|]);
  let bsb = ReasonCliTools.Commands.exec(
    ~cmd=bsbCommand ++ " -w",
    ~onOut=text => {
      print_endline(text);
      try {buildBundle(); print_endline("Rebuild the js bundle")} {
      | Failure(message) => failwith("Failed to rebuild js bundle: " ++ message)
      };
      ()
    }
  );
  let poll = () => {
    ReasonCliTools.Commands.poll(bsb);
  };
  let close = () => {
    ReasonCliTools.Commands.kill(bsb);
  };
  (poll, close)
}; */
