
let makeEnv = (~byte=false, arch, abi, ndk1, full) => {
  let cross = Filename.concat(Sys.getenv("HOME"), ".ocaml-cross-mobile");
  let ndk = try (Sys.getenv("ANDROID_NDK")) {
  | Not_found => cross ++ "/android-ndk"
  };

  let ocaml = cross ++ "/android-" ++ arch;
  let sysroot = ocaml;
  let darwin_ndk = ndk ++ "/toolchains/" ++ ndk1 ++ "-4.9/prebuilt/darwin-x86_64";
  let cc = darwin_ndk ++ "/bin/" ++ full ++ "-gcc  --sysroot " ++ ndk ++ "/platforms/android-24/arch-" ++ arch ++ " -I" ++ ndk ++ "/include -L" ++ ndk ++ "/lib -I" ++ ndk ++ "/sources/cxx-stl/gnu-libstdc++/4.9/include -I" ++ ndk ++ "/sources/cxx-stl/gnu-libstdc++/4.9/libs/" ++ abi ++ "/include -L" ++ ndk ++ "/sources/cxx-stl/gnu-libstdc++/4.9/libs/" ++ abi ++ " -I" ++ sysroot ++ "/include -L" ++ sysroot ++ "/lib";

  "OCAMLLIB=\"" ++ sysroot ++ "/lib/ocaml\" CAML_BYTERUN=\"" ++ sysroot ++ "/bin/ocamlrun\" CAML_BYTECC=\"" ++ cc ++ " -O2 -fno-defer-pop " ++ (byte ? "" : "-Wall-D_FILE_OFFSET_BITS=64 ") ++ "-D_REENTRANT -fPIC\" CAML_NATIVECC=\"" ++ cc ++ " -O2 -Wall -D_FILE_OFFSET_BITS=64 -D_REENTRANT -fPIC\" CAML_MKDLL=\"" ++ cc ++ " -O2 -shared\" CAML_MKMAINDLL=\"" ++ cc ++ " -O2 -shared\" CAML_MKEXE=\"" ++ cc ++ " -O2\" CAML_PACKLD=\"" ++ darwin_ndk ++ "/bin/" ++ full ++ "-ld --sysroot " ++ ndk ++ "/platforms/android-24/arch-" ++ arch ++ " -L" ++ ndk ++ "/lib -L" ++ ndk ++ "/sources/cxx-stl/gnu-libstdc++/4.9/libs/" ++ abi ++ " -L" ++ sysroot ++ "/lib -r  -o\" CAML_RANLIB=" ++ darwin_ndk ++ "/bin/" ++ full ++ "-ranlib CAML_ASM=" ++ darwin_ndk ++ "/bin/" ++ full ++ "-as";
};

let fixSettingsGradle = (settingsGradle, reasonglAndroidDir) => {
  let text = ReasonCliTools.Files.readFile(settingsGradle)
  |> Builder.unwrap("./android/settings.gradle not found!");
  let lines = Str.split(Str.regexp("\n"), text)
  |> List.map(
    line => switch (Str.search_forward(Str.regexp("reasongl-android/android"), line, 0)) {
    | exception Not_found => line
    | _ => "project(':reasongl-android').projectDir = new File(rootProject.projectDir, '" ++ String.escaped(Filename.concat(Filename.concat("..", reasonglAndroidDir), "android")) ++ "') // GENERATED"
    }
  );
  let contents = String.concat("\n", lines);
  ReasonCliTools.Files.writeFile(settingsGradle, contents) |> ignore;
};





/* let hotForArch = (entryFile, arch, ocamlarch, ndkarch, cxxarch, gccarch, gccarch2) => {
  let cross = Filename.concat(Sys.getenv("HOME"), ".ocaml-cross-mobile");
  let ndk = try (Sys.getenv("ANDROID_NDK")) {
  | Not_found => cross ++ "/android-ndk"
  };

  if (!Builder.exists("_build")) {
    failwith("_build doesn't exist -- you need to have done a full compilation first");
  };

  let ocaml = cross ++ "/android-" ++ ocamlarch;
  let env = makeEnv(~byte, ocamlarch, cxxarch, gccarch, gccarch2);
  if (!Builder.exists(ocaml)) {
    print_newline();
    print_endline("[!] OCaml compiler not found for android-" ++ ocamlarch ++ " (looked in " ++ ocaml ++ "). Please download from https://github.com/jaredly/ocaml-cross-mobile .");
    print_newline();
    exit(1);
  };
  if (!Builder.exists(ndk)) {
    print_endline("NDK not found. (looked in " ++ ndk ++ ". Please download 11c from https://developer.android.com/ndk/downloads/older_releases.html or specify its location with the ANDROID_NDK env variable.");
    exit(1);
  };

  if (!Builder.exists(entryFile)) {
    print_newline();
    print_endline("[!] no file " ++ entryFile ++ " found");
    print_newline();
    exit(1);
  };

  if (!Builder.exists("android")) {
    BuildUtils.copyDeep("./node_modules/reprocessing-scripts/templates/android", "./android");
  };

  let androidDir = BuildUtils.findNodeModule("@jaredly/reasongl-android", "./node_modules") |> Builder.unwrap("unable to find reasongl-android dependency");

  print_endline(Unix.getcwd());
  fixSettingsGradle("./android/settings.gradle", androidDir);

  Builder.compile(Builder.{
    name: "reasongl",
    byte,
    shared: true,
    mainFile: entryFile,
    cOpts: "-fno-omit-frame-pointer -O3 -fPIC -llog -landroid -lGLESv3 -lEGL --sysroot "
      ++ ndk ++ "/platforms/android-24/arch-" ++ ndkarch
      ++ " -I" ++ ndk ++ "/include -L"
      ++ ndk ++ "/lib -I"
      ++ ndk ++ "/sources/cxx-stl/gnu-libstdc++/4.9/include -I"
      ++ ndk ++ "/sources/cxx-stl/gnu-libstdc++/4.9/libs/" ++ cxxarch ++ "/include -L"
      ++ ndk ++ "/sources/cxx-stl/gnu-libstdc++/4.9/libs/" ++ cxxarch
      ++ " -I" ++ ocaml ++ "/include"
      ++ " -I" ++ ocaml ++ "/lib/ocaml -L"
      ++ ocaml ++ "/lib",
    mlOpts: "-runtime-variant _pic -g",
    dependencyDirs: [
      Filename.concat(BuildUtils.findNodeModule("@jaredly/reasongl-interface", "./node_modules") |> unwrap("unable to find reasongl-interface dependency"), "src"),
      Filename.concat(androidDir, "src"),
      Filename.concat(BuildUtils.findNodeModule("@jaredly/reprocessing", "./node_modules") |> unwrap("unable to find reprocessing dependency"), "src"),
    ],
    buildDir: "_build/android_" ++ arch,
    env: env ++ " BSB_BACKEND=" ++ (byte ? "byte-android" : "native-android"),

    cc: ndk ++ "/toolchains/" ++ gccarch ++ "-4.9/prebuilt/darwin-x86_64/bin/" ++ gccarch2 ++ "-gcc",
    outDir: "./android/app/src/main/jniLibs/" ++ arch ++ "/",
    ppx: [Filename.concat(BuildUtils.findMatchenv(), "matchenv")],
    ocamlDir: ocaml,
    refmt: "./node_modules/bs-platform/lib/refmt3.exe",
  });
}; */









let buildForArch = (~byte, entryFile, arch, ocamlarch, ndkarch, cxxarch, gccarch, gccarch2) => {
  let cross = Filename.concat(Sys.getenv("HOME"), ".ocaml-cross-mobile");
  let ndk = try (Sys.getenv("ANDROID_NDK")) {
  | Not_found => cross ++ "/android-ndk"
  };

  if (!Builder.exists("_build")) Unix.mkdir("_build", 0o740);

  let ocaml = cross ++ "/android-" ++ ocamlarch;
  let env = makeEnv(~byte, ocamlarch, cxxarch, gccarch, gccarch2);
  if (!Builder.exists(ocaml)) {
    print_newline();
    print_endline("[!] OCaml compiler not found for android-" ++ ocamlarch ++ " (looked in " ++ ocaml ++ "). Please download from https://github.com/jaredly/ocaml-cross-mobile .");
    print_newline();
    exit(1);
  };
  if (!Builder.exists(ndk)) {
    print_endline("NDK not found. (looked in " ++ ndk ++ ". Please download 11c from https://developer.android.com/ndk/downloads/older_releases.html or specify its location with the ANDROID_NDK env variable.");
    exit(1);
  };

  if (!Builder.exists(entryFile)) {
    print_newline();
    print_endline("[!] no file " ++ entryFile ++ " found");
    print_newline();
    exit(1);
  };

  if (!Builder.exists("android")) {
    BuildUtils.copyDeep("./node_modules/reprocessing-scripts/templates/android", "./android");
  };

  let androidDir = BuildUtils.findNodeModule("@jaredly/reasongl-android", "./node_modules") |> Builder.unwrap("unable to find reasongl-android dependency");

  print_endline(Unix.getcwd());
  fixSettingsGradle("./android/settings.gradle", androidDir);

  Builder.compile(Builder.{
    name: "reasongl",
    byte,
    shared: true,
    mainFile: entryFile,
    cOpts: "-fno-omit-frame-pointer -O3 -fPIC -llog -landroid -lGLESv3 -lEGL --sysroot "
      ++ ndk ++ "/platforms/android-24/arch-" ++ ndkarch
      ++ " -I" ++ ndk ++ "/include -L"
      ++ ndk ++ "/lib -I"
      ++ ndk ++ "/sources/cxx-stl/gnu-libstdc++/4.9/include -I"
      ++ ndk ++ "/sources/cxx-stl/gnu-libstdc++/4.9/libs/" ++ cxxarch ++ "/include -L"
      ++ ndk ++ "/sources/cxx-stl/gnu-libstdc++/4.9/libs/" ++ cxxarch
      ++ " -I" ++ ocaml ++ "/include"
      ++ " -I" ++ ocaml ++ "/lib/ocaml -L"
      ++ ocaml ++ "/lib",
    mlOpts: "-runtime-variant _pic -g",
    dependencyDirs: [
      Filename.concat(BuildUtils.findNodeModule("@jaredly/reasongl-interface", "./node_modules") |> unwrap("unable to find reasongl-interface dependency"), "src"),
      Filename.concat(androidDir, "src"),
      Filename.concat(BuildUtils.findNodeModule("@jaredly/reprocessing", "./node_modules") |> unwrap("unable to find reprocessing dependency"), "src"),
    ],
    buildDir: "_build/android_" ++ arch,
    env: env ++ " BSB_BACKEND=" ++ (byte ? "byte-android" : "native-android"),

    cc: ndk ++ "/toolchains/" ++ gccarch ++ "-4.9/prebuilt/darwin-x86_64/bin/" ++ gccarch2 ++ "-gcc",
    outDir: "./android/app/src/main/jniLibs/" ++ arch ++ "/",
    ppx: [Filename.concat(BuildUtils.findMatchenv(), "matchenv")],
    ocamlDir: ocaml,
    refmt: "./node_modules/bs-platform/lib/refmt3.exe",
  });
};

let armv7 = (~byte, entry) => buildForArch(~byte, entry, "armeabi-v7a", "armv7", "arm", "armabi", "arm-linux-androideabi", "arm-linux-androideabi");
let x86 = (~byte, entry) => buildForArch(~byte, entry, "x86", "x86", "x86", "x86", "x86", "i686-linux-android");

let both = () => {
  armv7(~byte=true, "./src/android.re");
  x86(~byte=true, "./src/android.re");
};

let assemble = () => {
  print_endline("Running ./gradlew assembleDebug");
  BuildUtils.showCommand(~echo=true, "cd android && ./gradlew assembleDebug")
};

let install = () => {
  print_endline("Running ./gradlew installDebug");
  if (!BuildUtils.showCommand(~echo=true, "cd android && ./gradlew installDebug")) {
    failwith("Gradle build failed");
  }
};

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

let hotServer = () => {
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
      print_endline("Compiling " ++ baseFile ++ ":D");

      armv7(~byte=true, baseFile);
      /* x86(~byte=true, baseFile); */

      let {Unix.st_size, st_perm} = Unix.stat("./hot.cma");

      let fs = Unix.openfile("./hot.cma", [Unix.O_RDONLY], st_perm);

      let buffer_size = 8192;
      let buffer = Bytes.create(buffer_size);

      let head = string_of_int(st_size) ++ ":";
      Bytes.blit_string(head, 0, buffer, 0, String.length(head));
      Unix.send(client, buffer, 0, String.length(head), []);
      /* let fd = Unix.openfile(dest, [Unix.O_WRONLY, Unix.O_CREAT, Unix.O_TRUNC], st_perm); */
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

      /* Unix.close(fd); */

      /* let response = "Hello folks";
      let total = String.length(response);
      let left = ref(String.length(response));
      while (left^ > 0) {
        left := left^ - Unix.send(client, response, total - left^, left^, []);
      }; */
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

let hot = () => {
  armv7(~byte=true, "./src/androidhot.re");
  x86(~byte=true, "./src/androidhot.re");
  install();
  hotServer();
};