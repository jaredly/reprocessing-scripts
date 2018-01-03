
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

let build = () => {
  let cwd = Unix.getcwd();
  if (!Builder.exists("public")) Unix.mkdir("public", 0o740);
  if (!Builder.exists("public/assets")) Unix.mkdir("public/assets", 0o740);
  if (!Builder.exists("public/index.html")) {
    let fd = Unix.openfile("public/index.html", [Unix.O_WRONLY, Unix.O_CREAT, Unix.O_TRUNC], 0o640);
    output_string(Unix.out_channel_of_descr(fd), basic_html);
    Unix.close(fd);
  };
  BuildUtils.readCommand("bsb -make-world -backend js") |> Builder.unwrap("Unable to run bsb") |> ignore;
  BuildUtils.readCommand("webpack --entry ./lib/js/src/web.js --output-path ./public/assets --output-filename bundle.js") |> Builder.unwrap("Unable to run webpack") |> ignore;
};
