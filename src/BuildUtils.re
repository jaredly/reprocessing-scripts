
let withReSuffix = path => Filename.chop_extension(path) ++ ".re";

let copy = (source, dest) => {
  let {Unix.st_perm} = Unix.stat(source);
  let fs = Unix.openfile(source, [Unix.O_RDONLY], st_perm);
  let fd = Unix.openfile(dest, [Unix.O_WRONLY, Unix.O_CREAT, Unix.O_TRUNC], st_perm);
  let buffer_size = 8192;
  let buffer = Bytes.create(buffer_size);
  let rec copy_loop = () => switch(Unix.read(fs, buffer, 0, buffer_size)) {
  | 0 => ()
  | r => {
    ignore(Unix.write(fd, buffer, 0, r)); copy_loop();
  }
  };
  copy_loop();
  Unix.close(fs);
  Unix.close(fd);
};

let exists = path => try {Unix.stat(path) |> ignore; true} {
| Unix.Unix_error(Unix.ENOENT, _, _) => false
| Unix.Unix_error(Unix.ENOTDIR, _, _) => false
};

let readdir = (dir) => {
  let maybeGet = (handle) =>
    try (Some(Unix.readdir(handle))) {
    | End_of_file => None
    };
  let rec loop = (handle) =>
    switch (maybeGet(handle)) {
    | None =>
      Unix.closedir(handle);
      []
    | Some(name) => [name, ...loop(handle)]
    };
  loop(Unix.opendir(dir))
};

let copyDirShallow = (source, dest) => {
  readdir(source)
  |> List.filter(name => name.[0] != '.')
  |> List.iter((name) => copy(Filename.concat(source, name), Filename.concat(dest, name)))
};

let rec mkdirp = (dest) => {
  if (!exists(dest)) {
    let parent = Filename.dirname(dest);
    mkdirp(parent);
    Unix.mkdir(dest, 0o740);
  }
};

let rec copyDeep = (source, dest) => {
  if (!exists(dest)) Unix.mkdir(dest, 0o740);
  readdir(source)
  |> List.filter(name => name != "." && name != "..")
  |> List.iter((name) => {
    let src = Filename.concat(source, name);
    switch (Unix.stat(src)) {
    | exception Unix.Unix_error(Unix.ENOENT, _, _) => ()
    | {Unix.st_kind: Unix.S_DIR} => copyDeep(src, Filename.concat(dest, name))
    | {Unix.st_kind: Unix.S_REG} => copy(src, Filename.concat(dest, name))
    | _ => print_endline("Skipping " ++ src)
    }
  })
};

/**
 * Get the output of a command, in lines.
 */
let readCommand = (command) => {
  print_endline(command);
  let chan = Unix.open_process_in(command);
  try {
    let rec loop = () => {
      switch (Pervasives.input_line(chan)) {
      | exception End_of_file => []
      | line => [line, ...loop()]
      }
    };
    let lines = loop();
    switch (Unix.close_process_in(chan)) {
    | WEXITED(0) => Some(lines)
    | WEXITED(_)
    | WSIGNALED(_)
    | WSTOPPED(_) =>
      print_endline("Command " ++ command ++ " ended with non-zero exit code");
      None
    }
  } {
  | End_of_file => None
  }
};

let expectSuccess = (message, (_, success)) => {
  if (!success) {
    failwith(message)
  } else {
    ()
  }
};

/**
 * Show the output of a command, in lines.
 */
let showCommand = (~echo=false, command) => {
  /* print_endline(command); */
  let chan = Unix.open_process_in(command);
  let output = ref("");
  try {
    let rec loop = () => {
      switch (Pervasives.input_line(chan)) {
      | exception End_of_file => ()
      | line => {
        if (echo) print_endline(line);
        output := output^ ++ line ++ "\n";
        loop()
      }
      }
    };
    loop();
    switch (Unix.close_process_in(chan)) {
    | WEXITED(0) => true
    | WEXITED(_)
    | WSIGNALED(_)
    | WSTOPPED(_) =>
      print_endline("Command " ++ command ++ " ended with non-zero exit code");
      print_newline();
      print_endline(output^);
      false
    }
  } {
  | End_of_file => false
  }
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
  (poll, close, desc)
};

let isAlive = pid => {
  let (p, status) = Unix.waitpid([Unix.WNOHANG, Unix.WUNTRACED], pid);
  p === 0
};

let showCommandOutput = (buffer, buffer_size, name, desc) => {
  let read = Unix.read(desc, buffer, 0, buffer_size);
  let got = Bytes.sub_string(buffer, 0, read);
  if (String.length(String.trim(got)) > 0) {
    print_endline("[ " ++ name ++ " ]");
    print_newline();
    print_endline("    " ++ Str.global_replace(Str.regexp("\n"), "\n    ", got));
  };
};

let keepAlive = (name, cmd, args) => {
  let buffer_size = 8192;
  let buffer = Bytes.create(buffer_size);

  let start = () => {
    print_endline(">> Starting " ++ name ++ " : " ++ cmd);
    let (r_in, w_in) = Unix.pipe();
    let (r_out, w_out) = Unix.pipe();
    let (r_err, w_err) = Unix.pipe();
    let pid = Unix.create_process(cmd, args, r_in, w_out, w_err);

    print_endline("PID: " ++ string_of_int(pid));
    /* let (poll, close, desc) = pollableCommand(name, cmd); */
    /* (poll, close, desc) */
    (pid, r_out, r_err)
  };

  let process = ref(start());
  let lastCheck = ref(Unix.gettimeofday());
  let poll = () => {
    let (pid, out, err) = process^;
    if (canRead(out)) {
      showCommandOutput(buffer, buffer_size, name, out)
    };
    if (canRead(err)) {
      showCommandOutput(buffer, buffer_size, name, err)
    };
    if (Unix.gettimeofday() -. lastCheck^ > 1.) {
      lastCheck := Unix.gettimeofday();
      if (!isAlive(pid)) {
        print_endline("<< Command died");
        process := start();
      }
    }
  };
  let close = () => {
    let (pid, out, err) = process^;
    Unix.kill(pid);
  };

  (poll, close)
};

let rec findNodeModule = (needle, base) => {
  if (exists(base) && ReasonCliTools.Files.isDirectory(base)) {
    let full = Filename.concat(base, needle);
    if (ReasonCliTools.Files.isDirectory(full)) {
      Some(full)
    } else {
      let names = ReasonCliTools.Files.readDirectory(base);
      let rec loop = names => {
        switch (names) {
        | [name, ...rest] => {
          let child = name.[0] == '@' ? name : Filename.concat(name, "node_modules");
          switch (findNodeModule(needle, Filename.concat(base, child))) {
          | None => loop(rest)
          | Some(x) => Some(x)
          }
        }
        | [] => None
      }
      };
      loop(names)
    }
  } else {
    None
  }
};

let findMatchenv = () => {
  switch (findNodeModule("matchenv", "node_modules")) {
  | None => failwith("matchenv not found - it should be installed as a dependency of reprocessing")
  | Some(x) => x
  }
};
