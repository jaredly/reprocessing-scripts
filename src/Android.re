
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





let configForArch = (~byte, entryFile, arch, ocamlarch, ndkarch, cxxarch, gccarch, gccarch2) => {
  let cross = Filename.concat(Sys.getenv("HOME"), ".ocaml-cross-mobile");
  let ndk = try (Sys.getenv("ANDROID_NDK")) {
  | Not_found => cross ++ "/android-ndk"
  };

  let ocaml = cross ++ "/android-" ++ ocamlarch;
  let env = makeEnv(~byte, ocamlarch, cxxarch, gccarch, gccarch2);

  let androidDir = BuildUtils.findNodeModule("@jaredly/reasongl-android", "./node_modules") |> Builder.unwrap("unable to find reasongl-android dependency");

  Builder.{
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
    env: env ++ " BSB_BACKEND=" ++ (byte ? "byte-android" : "native-android") ++ " LOCAL_IP=" ++ HotServer.myIp() ++ " RSB_ARCH=" ++ ocamlarch,

    cc: ndk ++ "/toolchains/" ++ gccarch ++ "-4.9/prebuilt/darwin-x86_64/bin/" ++ gccarch2 ++ "-gcc",
    outDir: "./android/app/src/main/jniLibs/" ++ arch ++ "/",
    ppx: [
      Filename.concat(BuildUtils.findMatchenv(), "matchenv"),
      Filename.concat(BuildUtils.findPpxEnv(), "ppx-env"),
    ],
    ocamlDir: ocaml,
    refmt: "./node_modules/bs-platform/lib/refmt3.exe",
  };
};



let build = (config) => {
  open Builder;

  /* let config = configForArch(~byte, entryFile, arch, ocamlarch, ndkarch, cxxarch, gccarch, gccarch2); */

  if (!Builder.exists("_build")) Unix.mkdir("_build", 0o740);

  if (!Builder.exists(config.ocamlDir)) {
    print_newline();
    print_endline("[!] OCaml cross-compiler  (looked in " ++ config.ocamlDir ++ "). Please download from https://github.com/jaredly/ocaml-cross-mobile .");
    print_newline();
    exit(1);
  };

  if (!Builder.exists(config.cc)) {
    print_endline("NDK not found. (looked in " ++ config.cc ++ ". Please download 11c from https://developer.android.com/ndk/downloads/older_releases.html or specify its location with the ANDROID_NDK env variable.");
    exit(1);
  };

  if (!Builder.exists(config.mainFile)) {
    print_newline();
    print_endline("[!] no file " ++ config.mainFile ++ " found");
    print_newline();
    exit(1);
  };

  if (!Builder.exists("android")) {
    BuildUtils.copyDeep("./node_modules/reprocessing-scripts/templates/android", "./android");
  };

  let androidDir = BuildUtils.findNodeModule("@jaredly/reasongl-android", "./node_modules") |> Builder.unwrap("unable to find reasongl-android dependency");
  fixSettingsGradle("./android/settings.gradle", androidDir);

  Builder.compile(config);
};

let armv7Config = (~byte, entry) => {
  configForArch(~byte, entry, "armeabi-v7a", "armv7", "arm", "armabi", "arm-linux-androideabi", "arm-linux-androideabi");
};
let x86Config = (~byte, entry) => {
  configForArch(~byte, entry, "x86", "x86", "x86", "x86", "x86", "i686-linux-android");
};

let both = () => {
  build(armv7Config(~byte=false, "./src/android.re"));
  build(x86Config(~byte=false, "./src/android.re"));
};

let assemble = () => {
  print_endline("Running ./gradlew assembleDebug");
  BuildUtils.showCommand(~echo=true, "cd android && ./gradlew assembleDebug")
};

let install = () => {
  print_endline("Running ./gradlew installDebug");
  if (!BuildUtils.showCommand(~echo=true, "cd android && ./gradlew installDebug")) {
    failwith("Gradle build failed");
  };
};

let getString = (message, t) => switch t {
| Json.String(s) => s
| _ => failwith(message)
};

let run = (config) => {
  print_endline("Running");
  let appId = config |> Json.get("androidPackage")
  |> Builder.unwrap("No androidPackage in bsconfig.json")
  |> getString("Expected androidPackage to be a string in bsconfig.json");
  if (!BuildUtils.showCommand(~echo=true, "adb shell monkey -p " ++ appId ++ " 1")) {
    failwith("Unable to start app");
  }
};

let compileLib = mainFile => {
  let config = armv7Config(~byte=true, mainFile);
  build(config);

  let mainDir = Filename.dirname(mainFile);
  let siblings = ReasonCliTools.Files.readDirectory(mainDir) |> List.map(Filename.concat(mainDir));
  let mainFileName = Filename.concat(config.buildDir, Filename.basename(mainFile));
  let reasonOrOcamlFiles = List.filter(Builder.isSourceFile, siblings);
  let filesInOrder = Builder.unwrap("Failed to run ocamldep", Getdeps.sortSourceFilesInDependencyOrder(~ocamlDir=config.ocamlDir, ~refmt=config.refmt, ~ppx=config.ppx, ~env=config.env, reasonOrOcamlFiles, mainFile));

  let cmos = List.map(n => Filename.concat(config.buildDir, Filename.chop_extension(Filename.basename(n)) ++ ".cmo"), filesInOrder)
  ;
  print_endline("Hot files: " ++ String.concat(" - ", cmos));
  let dest = "android-hot.cma";
  BuildUtils.readCommand(Printf.sprintf(
    "%s %s -a %s -o %s",
    Filename.concat(config.ocamlDir, "bin/ocamlrun"),
    Filename.concat(config.ocamlDir, config.byte ? "bin/ocamlc" : "bin/ocamlopt"),
    String.concat(" ", cmos),
    dest
  )) |> Builder.unwrap("Failed to make lib") |> ignore;
  (dest, filesInOrder)
};

let hot = (config) => {
  build(armv7Config(~byte=true, "./src/androidhot.re"));
  build(x86Config(~byte=true, "./src/androidhot.re"));
  install();
  run(config);
  HotServer.hotServer(compileLib);
  /* hotServer(); */
};