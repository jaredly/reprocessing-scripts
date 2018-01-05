
let readall = (fd) => {
  let rec loop = () => switch (Pervasives.input_line(fd)) {
  | exception End_of_file => []
  | line => [line, ...loop()]
  };
  loop();
};

let exists = path => try {Unix.stat(path) |> ignore; true} {
| Not_found => false
};

let readFile = path => {
  let fd = Unix.openfile(path, [Unix.O_RDONLY], 0o640);
  let text = String.concat("\n", readall(Unix.in_channel_of_descr(fd)));
  Unix.close(fd);
  text
};

let mime_for_name = ext => switch (String.lowercase(ext)) {
| "txt" => "text/plain"
| "html" => "text/html"
| "json" => "text/json"
| "js" => "application/javascript"
| "jpg" | "jpeg" => "image/jpeg"
| "png" => "image/png"
| "pdf" => "image/pdf"
| "ico" => "image/ico"
| "gif" => "image/gif"
| _ => "application/binary"
};

let ext = path => {
  let parts = Str.split(Str.regexp("\\."), path);
  List.nth(parts, List.length(parts) - 1)
};

let isFile = path => switch (Unix.stat(path)) {
| exception Unix.Unix_error(Unix.ENOENT, _, _) => false
| {Unix.st_kind: Unix.S_REG} => true
| _ => false
};

open BasicServer.Response;
let serveStatic = (full_path, path) => {
  switch (Unix.stat(full_path)) {
  | exception Unix.Unix_error(Unix.ENOENT, _, _) => Bad(404, "File not found: " ++ path)
  | stat =>
  switch (stat.Unix.st_kind) {
  | Unix.S_REG => Ok(mime_for_name(ext(path)), readFile(full_path))
  | Unix.S_DIR => {
    let index = Filename.concat(full_path, "index.html");
    if (isFile(index)) {
      Ok("text/html", readFile(index))
    } else {
      Ok("text/plain", "Directory")
    }
  }
  | _ => Bad(404, "Unexpected file type: " ++ path)
  };
  }
};

let handler = (base, method, path, headers) => {
  switch (method) {
  | "GET" => {
    let full_path = Filename.concat(base, "." ++ path);
    serveStatic(full_path, path)
  }
  | _ => Bad(401, "Method not allowed: " ++ method)
  }
};

let run = (~poll=?, path) => BasicServer.listen(~poll=?poll, 3451, handler(path));

let main = () => {
  switch (Sys.argv) {
  | [|_|] => BasicServer.listen(3451, handler("./"))
  | [|_, path|] => BasicServer.listen(3451, handler(path))
  | _ => print_endline("Usage: serve [path: default current]")
  }
};
