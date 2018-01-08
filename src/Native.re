
let byte = () => BuildUtils.showCommand("./node_modules/.bin/bsb -make-world -backend bytecode") |> ignore;
let run = () => BuildUtils.showCommand("./node_modules/.bin/bsb -make-world -backend native") |> ignore;

let maybeFind = (fn, lst) => {
  switch (List.filter(fn, lst)) {
  | [] => None
  | [first, ..._] => Some(first)
  }
};

let (|>>) = (v, f) => switch v { | None => None | Some(v) => f(v) };
let getHotName = config => {
  open Json;
  config |> get("entries") |>> (arr => switch arr {
  | Array(items) => {
    let byte = List.filter(i => get("backend", i) == Some(String("bytecode")), items);
    let hot = maybeFind(i => get("hot", i) == Some(True), byte);
    let got = switch hot {
    | None => maybeFind(i => get("main-module", i) == Some(String("Indexhot")), byte)
    | Some(i) => Some(i)
    };
    got |>> get("main-module") |>> (v => switch v { | String(v) => Some(v) | _ => None})
  }
  | _ => None
  })
};

let hot = (config) => {
  byte() |> ignore;

  let (cmd, closecmd, _) = BuildUtils.pollableCommand("Bucklescript", "./node_modules/.bin/bsb -make-world -backend bytecode -w");
  let (poll, close) = BuildUtils.keepAlive("Game", "bash", [|"bash", "-c", "./lib/bs/bytecode/indexhot.byte"|]);
  let lastCheck = ref(Unix.gettimeofday());
  while (true) {
    cmd();
    poll();
  };
};

let infoPlist = (name) => {|
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleGetInfoString</key>
  <string>|} ++ name ++ {|</string>
  <key>CFBundleExecutable</key>
  <string>|} ++ name ++ {|</string>
  <key>CFBundleIdentifier</key>
  <string>com.example.www</string>
  <key>CFBundleName</key>
  <string>|} ++ name ++ {|</string>
  <key>CFBundleIconFile</key>
  <string>icon.icns</string>
  <key>CFBundleShortVersionString</key>
  <string>0.01</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>IFMajorVersion</key>
  <integer>0</integer>
  <key>IFMinorVersion</key>
  <integer>1</integer>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSSupportsAutomaticGraphicsSwitching</key><true/>
</dict>
</plist>
|};

let sips = (icon, size, suffix) => {
  let cmd = Printf.sprintf(
    "sips -z %d %d \"%s\" --out icon.iconset/icon_%s.png",
    size,
    size,
    icon,
    suffix
  );
  ReasonCliTools.Commands.execSync(~cmd, ()) |> ignore
};

let maybeString = (t) => switch t {
| Json.String(s) => Some(s)
| _ => None
};

let optBind = (f, x) => switch x { | None => None | Some(x) => f(x) };
let optOr = (o, x) => switch x { | None => o | Some(x) => x };

let getString = (message, t) => switch t {
| Json.String(s) => s
| _ => failwith(message)
};

let expectSuccess = (message, (_, success)) => {
  if (!success) {
    failwith(message)
  } else {
    ()
  }
};

open ReasonCliTools;
let makeIcon = (config, base) => {

  Files.mkdirp("icon.iconset");

  let icon = config |> Json.get("icon")
  |> Builder.unwrap("No icon in bsconfig.json")
  |> getString("Expected icon to be a string in bsconfig.json");

  let sips2 = (num) => {
    let s = string_of_int(num);
    sips(icon, num, s ++ "x" ++ s);
    sips(icon, num * 2, s ++ "x" ++ s ++ "@2");
  };
  sips2(16);
  sips2(32);
  sips2(64);
  sips2(128);
  sips2(256);
  sips2(512);

  Commands.execSync(~cmd="iconutil -c icns icon.iconset", ()) |> expectSuccess("Failed to make icon");
  Files.removeDeep("icon.iconset");
  Files.copy(~source="icon.icns", ~dest=base ++ "/Contents/Resources/icon.icns") |> ignore;
  Files.removeDeep("icon.icns");

};

let fixSharedSDL = (executable, base) => {
  let (out, success) = Commands.execSync(~cmd="otool -L " ++ executable ++ " | grep libSDL | sed -e 's/(.*//' | sed -e 's/[[:space:]]//g'", ());
  if (!success) {
    failwith("Unable to find sdl path");
  };
  let sdl = switch out {
  | [line] => line
  | _ => failwith("Unable to find sdl path")
  };
  Commands.execSync(~cmd="install_name_tool -change " ++ sdl ++ " @executable_path/libSDL2-2.0.0.dylib " ++ executable, ()) |> expectSuccess("Unable to fix sdl path");

  Files.copy(~source=sdl, ~dest=base ++ "/Contents/MacOS/libSDL2-2.0.0.dylib") |> ignore;
};

let bundle = (config) => {
  let name = config |> Json.get("name")
  |> Builder.unwrap("No name in bsconfig.json")
  |> getString("Expected name to be a string in bsconfig.json");
  let appName = config |> Json.get("appName") |> optBind(maybeString) |> optOr(name);
  let base = appName ++ ".app";
  Files.removeDeep(base);
  Files.mkdirp(base ++ "/Contents/MacOS");
  Files.mkdirp(base ++ "/Contents/Resources");

  Files.writeFile(base ++ "/Contents/Info.plist", infoPlist(appName)) |> ignore;

  makeIcon(config, base);

  let executable = base ++ "/Contents/MacOS/" ++ appName;
  Files.copy(~source="./lib/bs/native/prod.native", ~dest=executable) |> ignore;

  fixSharedSDL(executable, base);
  Files.copyDeep(~source="assets", ~dest=base ++ "/Contents/MacOS/assets");

  ()
};