
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

let webpackCommand = "webpack --entry ./lib/js/src/web.js \
    --resolve-alias Reprocessing=@jaredly/reprocessing \
    --resolve-alias ReasonglInterface=@jaredly/reasongl-interface \
    --resolve-alias ReasonglWeb=@jaredly/reasongl-web \
    --output-path ./public/assets \
    --output-filename bundle.js";

let bsbCommand = "bsb -make-world -backend js";

let build = () => {
  ensurePublic();
  print_endline(">> Running bsb");
  print_newline();
  BuildUtils.readCommand(bsbCommand) |> Builder.unwrap("Unable to run bsb") |>  String.concat("\n    ") |> (x) => print_endline("\n    " ++ x);
  print_newline();
  print_endline(">> Running webpack");
  print_newline();
  BuildUtils.readCommand(webpackCommand) |> Builder.unwrap("Unable to run webpack") |> String.concat("\n    ") |> (x) => print_endline("\n    " ++ x);
  if (Builder.exists("assets")) {
    print_newline();
    print_endline(">> Copying assets");
    BuildUtils.copyDirShallow("./assets", "./public/assets");
  };
  print_newline();
  print_endline(">> Js build done <<")
};

let canRead = desc => {
  let (r, w, e) = Unix.select([desc], [], [], 0.01);
  r != []
};

let pollableCommand = (name, cmd) => {
  let proc = Unix.open_process_in(cmd);
  let desc = Unix.descr_of_in_channel(proc);
  let out = ref("");
  let buffer_size = 8192;
  let buffer = Bytes.create(buffer_size);
  let poll = () => {
    if (canRead(desc)) {
      let read = Unix.read(desc, buffer, 0, buffer_size);
      let got = Bytes.sub_string(buffer, 0, read);
      if (String.length(String.trim(got)) > 0) {
        print_endline("[ " ++ name ++ " ]");
        print_newline();
        print_endline("    " ++ Str.global_replace(Str.regexp("\n"), "\n    ", got));
      };
      out := out^ ++ got;
    }
  };
  let close = () => {
    Unix.close_process_in(proc) |> ignore
  };
  (poll, close)
};

let watch = () => {
  ensurePublic();
  if (Builder.exists("assets")) {
    print_endline(">> Copying assets");
    BuildUtils.copyDirShallow("./assets", "./public/assets");
  };
  let buffers = ref([|"", ""|]);
  let (bsb, close_bsb) = pollableCommand("Bucklescript", bsbCommand ++ " -w");
  let (webpack, close_webpack) = pollableCommand("Webpack", webpackCommand ++ " -w");
  let poll = () => {
    bsb();
    webpack();
  };
  let close = () => {
    close_bsb();
    close_webpack();
  };
  (poll, close)
};
