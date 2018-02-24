
let makeEnv = (cross, xcode, arch) => {
  let ocaml = cross ++ "/ios-" ++ arch;
  let sysroot = ocaml;
  let cc = "clang -arch " ++ arch ++ " -isysroot " ++ xcode ++ "/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk -miphoneos-version-min=8.0";

  "OCAMLLIB=\"" ++ sysroot ++ "/lib/ocaml\"
   CAML_BYTERUN=\"" ++ sysroot ++ "/bin/ocamlrun\"
   CAML_BYTECC=\"" ++ cc ++ " -O2 -Wall\"
   CAML_NATIVECC=\"" ++ cc ++ " -O2 -Wall\"
   CAML_MKEXE=\"" ++ cc ++ " -O2\"
   CAML_ASM=\"" ++ cc ++ " -c\""
   ++ "              ";
};

let buildForArch = (~byte=false, ~suffixed=true, bsconfig, cross, xcode, arch, sdkName) => {
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

  let (packagedLibs, dependencyDirs) = BsConfig.processDeps(bsconfig);

  Builder.compile(Builder.{
    name: suffixed ? "reasongl_" ++ arch : "reasongl",
    byte,
    shared: false,
    mainFile: "./src/ios.re",
    cOpts: "-arch " ++ arch ++ " -isysroot " ++ sdk ++ " -isystem " ++ ocaml ++ "/lib/ocaml -DCAML_NAME_SPACE -I" ++ Filename.concat(iosDir, "ios") ++ " -I" ++ ocaml ++ "/lib/ocaml/caml -fno-objc-arc -miphoneos-version-min=7.0",
    mlOpts: "",
    /* (byte ? "bigarray.cma dynlink.cma unix.cma" : "bigarray.cmxa"), */
    dependencyDirs: [
      Filename.concat(BuildUtils.findNodeModule("@jaredly/reasongl-interface", "./node_modules") |> unwrap("unable to find reasongl-interface dependency"), "src"),
      Filename.concat(iosDir, "src"),
      Filename.concat(BuildUtils.findNodeModule("@jaredly/reprocessing", "./node_modules") |> unwrap("unable to find reprocessing dependency"), "src"),
    ] @ dependencyDirs,
    packagedLibs,
    buildDir: "_build/ios_" ++ arch,
    env: makeEnv(cross, xcode, arch) ++ " BSB_BACKEND=" ++ (byte ? "byte-ios" : "native-ios") ++ " LOCAL_IP=" ++ HotServer.myIp(),

    cc: xcode ++ "/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang",
    outDir: "./ios/",
    ppx: [
      Filename.concat(BuildUtils.findMatchenv(), "matchenv"),
      Filename.concat(BuildUtils.findPpxEnv(), "ppx-env"),
    ],
    ocamlDir: ocaml,
    refmt: "./node_modules/bs-platform/lib/refmt3.exe",
  });
};

let arm64 = (bsconfig) => {
  let cross = Filename.concat(Sys.getenv("HOME"), ".ocaml-cross-mobile");
  let xcode = BuildUtils.readCommand("xcode-select -p") |> Builder.unwrap("Failed to find xcode") |> List.hd;
  buildForArch(~suffixed=false, bsconfig, cross, xcode, "arm64", "iPhoneOS");
};
let x86_64 = (bsconfig) => {
  let cross = Filename.concat(Sys.getenv("HOME"), ".ocaml-cross-mobile");
  let xcode = BuildUtils.readCommand("xcode-select -p") |> Builder.unwrap("Failed to find xcode") |> List.hd;
  buildForArch(~suffixed=false, bsconfig, cross, xcode, "x86_64", "iPhoneSimulator");
};

let both = (bsconfig) => {
  let cross = Filename.concat(Sys.getenv("HOME"), ".ocaml-cross-mobile");
  let xcode = BuildUtils.readCommand("xcode-select -p") |> Builder.unwrap("Failed to find xcode") |> List.hd;

  if (!Builder.exists("_build")) Unix.mkdir("_build", 0o740);

  buildForArch(bsconfig, cross, xcode, "x86_64", "iPhoneSimulator");
  buildForArch(bsconfig, cross, xcode, "arm64", "iPhoneOS");

  BuildUtils.readCommand(
    "lipo -create -o ios/libreasongl.a ios/libreasongl_arm64.a ios/libreasongl_x86_64.a"
  ) |> Builder.unwrap("unable to link together") |> ignore;

  Unix.unlink("ios/libreasongl_arm64.a");
  Unix.unlink("ios/libreasongl_x86_64.a");
};

let isLink = (path) => switch (Unix.readlink(path)) {
| exception _ => false
| _ => true
};

let ensureSymlink = () => {
  let iosDir = BuildUtils.findNodeModule("reasongl-ios", "node_modules") |> Builder.unwrap("Package reasongl-ios not found");
  if (isLink("ios/reprocessing")) {
    Unix.unlink("ios/reprocessing");
  };
  Unix.symlink(Filename.concat("..", Filename.concat(iosDir, "ios")), "ios/reprocessing");
};

let getAppName = config => {
  let (|>>) = Json.bind;
  config |> Json.get("iosPathName")
  |>> Json.string
  |> fun
  | None => "App"
  | Some(x) => x
};

let buildForDevice = (bsconfig) => {
  ensureSymlink();
  let appName = getAppName(bsconfig);
  ReasonCliTools.Commands.execSync(
    ~onOut=print_endline,
    ~cmd="cd ios && xcodebuild -project=" ++ appName ++ ".xcodeproj -configuration Debug -arch arm64 -sdk iphoneos build",
    ()
  ) |> BuildUtils.expectSuccess("Unable to build for device");
  ReasonCliTools.Commands.execSync(
    ~onOut=print_endline,
    ~cmd="ios-deploy --bundle ios/_build/Debug-iphoneos/" ++ appName ++ ".app --justlaunch",
    ()
  ) |> BuildUtils.expectSuccess("Unable to launch on device");
};

let xcodebuild = (bsconfig) => {
  ensureSymlink();
  let appName = getAppName(bsconfig);
  ReasonCliTools.Commands.execSync(
    ~onOut=print_endline,
    ~cmd="cd ios && xcodebuild -project=" ++ appName ++ ".xcodeproj -configuration Debug -arch x86_64 -sdk iphonesimulator build",
    ()
  ) |> BuildUtils.expectSuccess("Unable to build for simulator");
};

let startSimulator = (bsconfig) => {
  let appName = getAppName(bsconfig);
  print_endline("Starting simulator");
  ReasonCliTools.Commands.execSync(
    ~onOut=print_endline,
    ~cmd="ios-sim launch ios/_build/Debug-iphonesimulator/" ++ appName ++ ".app --log ./ios.log --devicetypeid 'iPhone-8, 11.2'",
    ()
  ) |> BuildUtils.expectSuccess("Unable to start simulator");
};