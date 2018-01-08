
let makeEnv = (cross, xcode, arch) => {
  let ocaml = cross ++ "/ios-" ++ arch;
  let sysroot = ocaml;
  let cc = "clang -arch " ++ arch ++ " -isysroot " ++ xcode ++ "/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk -miphoneos-version-min=8.0";

  "OCAMLLIB=\"" ++ sysroot ++ "/lib/ocaml\"
   CAML_BYTERUN=\"" ++ sysroot ++ "/bin/ocamlrun\"
   CAML_BYTECC=\"" ++ cc ++ " -O2 -Wall\"
   CAML_NATIVECC=\"" ++ cc ++ " -O2 -Wall\"
   CAML_MKEXE=\"" ++ cc ++ " -O2\"
   CAML_ASM=\"" ++ cc ++ " -c\"";
};

let buildForArch = (~suffixed=true, cross, xcode, arch, sdkName) => {
  let sdk = xcode ++ "/Platforms/" ++ sdkName ++ ".platform/Developer/SDKs/" ++ sdkName ++ ".sdk";

  let ocaml = cross ++ "/ios-" ++ arch;
  if (!Builder.exists(ocaml)) {
    print_newline();
    print_endline("[!] OCaml compiler not found for ios-" ++ arch ++ " (looked in " ++ ocaml ++ "). Please download from https://github.com/jaredly/ocaml-cross-mobile .");
    print_newline();
    exit(1);
  };

  if (!Builder.exists("./src/ios.re")) {
    print_newline();
    print_endline("[!] no file ./src/ios.re found");
    print_newline();
    exit(1);
  };

  if (!Builder.exists("ios")) {
    BuildUtils.copyDeep("./node_modules/reprocessing-scripts/templates/ios", "./ios");
  };

  let iosDir = BuildUtils.findNodeModule("@jaredly/reasongl-ios", "./node_modules") |> Builder.unwrap("unable to find reasongl-ios dependency");

  Builder.compile(Builder.{
    name: suffixed ? "reasongl_" ++ arch : "reasongl",
    shared: false,
    mainFile: "./src/ios.re",
    cOpts: "-arch " ++ arch ++ " -isysroot " ++ sdk ++ " -isystem " ++ ocaml ++ "/lib/ocaml -DCAML_NAME_SPACE -I" ++ Filename.concat(iosDir, "ios") ++ " -I" ++ ocaml ++ "/lib/ocaml/caml -fno-objc-arc -miphoneos-version-min=7.0",
    mlOpts: "bigarray.cmxa",
    dependencyDirs: [
      Filename.concat(BuildUtils.findNodeModule("@jaredly/reasongl-interface", "./node_modules") |> unwrap("unable to find reasongl-interface dependency"), "src"),
      Filename.concat(iosDir, "src"),
      Filename.concat(BuildUtils.findNodeModule("@jaredly/reprocessing", "./node_modules") |> unwrap("unable to find reprocessing dependency"), "src"),
    ],
    buildDir: "_build/ios_" ++ arch,
    env: makeEnv(cross, xcode, arch) ++ " BSB_BACKEND=native-ios",

    cc: xcode ++ "/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang",
    outDir: "./ios/",
    ppx: [Filename.concat(BuildUtils.findMatchenv(), "matchenv")],
    ocamlDir: ocaml,
    refmt: "./node_modules/bs-platform/lib/refmt3.exe",
  });
};

let arm64 = () => {
  let cross = Filename.concat(Sys.getenv("HOME"), ".ocaml-cross-mobile");
  let xcode = BuildUtils.readCommand("xcode-select -p") |> Builder.unwrap("Failed to find xcode") |> List.hd;
  buildForArch(~suffixed=false, cross, xcode, "arm64", "iPhoneOS");
};
let x86_64 = () => {
  let cross = Filename.concat(Sys.getenv("HOME"), ".ocaml-cross-mobile");
  let xcode = BuildUtils.readCommand("xcode-select -p") |> Builder.unwrap("Failed to find xcode") |> List.hd;
  buildForArch(~suffixed=false, cross, xcode, "x86_64", "iPhoneSimulator");
};

let both = () => {
  let cross = Filename.concat(Sys.getenv("HOME"), ".ocaml-cross-mobile");
  let xcode = BuildUtils.readCommand("xcode-select -p") |> Builder.unwrap("Failed to find xcode") |> List.hd;

  if (!Builder.exists("_build")) Unix.mkdir("_build", 0o740);

  buildForArch(cross, xcode, "x86_64", "iPhoneSimulator");
  buildForArch(cross, xcode, "arm64", "iPhoneOS");

  BuildUtils.readCommand(
    "lipo -create -o ios/libreasongl.a ios/libreasongl_arm64.a ios/libreasongl_x86_64.a"
  ) |> Builder.unwrap("unable to link together") |> ignore;

  Unix.unlink("ios/libreasongl_arm64.a");
  Unix.unlink("ios/libreasongl_x86_64.a");
};

let buildForDevice = () => {
  "xcodebuild -project=App.xcodeproj -configuration Debug -arch arm64 -sdk iphoneos clean build";
  "ios-deploy --bundle _build/Debug-iphoneos/App.app --justlaunch"
};

let xcodebuild = () => {
  "xcodebuild -project=App.xcodeproj -configuration Debug -arch x86_64 -sdk iphonesimulator clean build";
  failwith("TODO impl");
};

let startSimulator = () => {
  "ios-sim launch _build/Debug-iphonesimulator/App.app --devicetypeid 'iPhone-8, 11.1'";
  failwith("TODO impl");
};