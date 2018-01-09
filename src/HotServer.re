/*
 * This is not a web-server, it's a hot server :D
 *
 * Client connects, and sends a message, indicating the platform, and the base file
 * e.g.
 * "ios:./src/ios.re\n"
 * or
 * "android:./src/android.re\n"
 *
 * The server then pushes updates, 0 - infinity, as long as the socket is alive.
 * It watches source files, and when there's an update it recompiles the stuff,
 * creates a .cma, and sends it over prepended by length as an int, and then the contents.
 */

let recv = (client, maxlen) => {
  let bytes = Bytes.create(maxlen);
  let len = Unix.recv(client, bytes, 0, maxlen, []);
  Bytes.sub_string(bytes, 0, len)
};

let canRead = desc => {
  let (r, w, e) = Unix.select([desc], [], [], 0.1);
  r != []
};

let stripNewline = n => {
  if (n.[String.length(n) - 1] == '\n') {
    String.sub(n, 0, String.length(n) - 1)
  } else {
    n
  }
};

let sendFile = (client, libPath) => {
  let {Unix.st_size, st_perm} = Unix.stat(libPath);
  let fs = Unix.openfile(libPath, [Unix.O_RDONLY], st_perm);

  let buffer_size = 8192;
  let buffer = Bytes.create(buffer_size);

  let head = string_of_int(st_size) ++ ":";
  Bytes.blit_string(head, 0, buffer, 0, String.length(head));
  Unix.send(client, buffer, 0, String.length(head), []) |> ignore;
  let rec copy_loop = () =>
    switch (Unix.read(fs, buffer, 0, buffer_size)) {
    | 0 => ()
    | r =>
      let left = ref(r);
      while (left^ > 0) {
        left := left^ - Unix.send(client, buffer, r - left^, left^, []);
      };
      copy_loop()
    };
  copy_loop();
  Unix.close(fs);

};

let minisleep = (sec: float) => ignore(Unix.select([], [], [], sec));


let needsRebuild = (lastModifiedTimes, fileNames) =>
  List.fold_left(
    (needsRebuild, name) => {
      /* If a file hasn't been compiled that we expect to be there, we set `notReady` to true
      * As soon as bucklescript has built it, it will be ready. */
      let mtime = try (Some(Unix.stat(name).Unix.st_mtime)) {
      | Unix.Unix_error(Unix.ENOENT, _, _) => {
        print_endline("File does not exist " ++ name);
        None
      }
      };
      switch mtime {
      | None => true
      | Some(st_mtime) =>
        if (Hashtbl.mem(lastModifiedTimes, name)) {
          if (st_mtime > Hashtbl.find(lastModifiedTimes, name)) {
            Hashtbl.add(lastModifiedTimes, name, st_mtime);
            true
          } else {
            needsRebuild
          }
        } else {
          Hashtbl.add(lastModifiedTimes, name, st_mtime);
          true
        }
      }
    },
    false,
    fileNames
  );

let watchFiles = (files) => {
  let lastModifiedTimes = Hashtbl.create(10);
  List.iter(name => Hashtbl.add(lastModifiedTimes, name, Unix.stat(name).Unix.st_mtime), files);

  while(!needsRebuild(lastModifiedTimes, files)) {
    minisleep(0.1);
  };

  /* minisleep(0.1); */
};

let hotServer = (compile) => {
  let port = 8090;
  let sock = Unix.socket(Unix.PF_INET, Unix.SOCK_STREAM, 0);
  Unix.setsockopt(sock, Unix.SO_REUSEADDR, true);
  Unix.bind(sock, Unix.ADDR_INET(Unix.inet_addr_any, port));
  Unix.listen(sock, 1000);
  print_endline("Listening");

  let pollSock = () => {
    print_endline("accepting");
    let (client, source) = Unix.accept(sock);
    print_endline("acceped");
    let request = recv(client,  1024);
    print_endline("Got request " ++ request);

    let parts = Str.split(Str.regexp(":"), request);
    switch (parts) {
    | ["android", baseFile] => {
      let baseFile = stripNewline(baseFile);

      let rec loop = (filesToWatch) => {
        print_endline("Compiling " ++ baseFile ++ ":D");
         switch (compile(baseFile)) {
         | exception err => {
           watchFiles(filesToWatch);
           loop(filesToWatch);
         }
         | (libPath, filesToWatch) =>
         print_endline("Files to watch: " ++ String.concat(" - ", filesToWatch));
          let died = try { sendFile(client, libPath); false } { | _ => true };
          Unix.unlink(libPath);
          if (!died) {
            watchFiles(filesToWatch);
            loop(filesToWatch);
          }
         }
      };

      loop([baseFile]);

      Unix.shutdown(client, Unix.SHUTDOWN_ALL);

    }
    | _ => {
      print_endline("Bad request: " ++ request);
      Unix.shutdown(client, Unix.SHUTDOWN_ALL);
    }
    }

  };

  while (true) {
    pollSock();

    /* switch(poll) {
    | Some(fn) => fn();
    | None => ()
    }; */
  }
};
