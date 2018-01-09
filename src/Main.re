
let usage = {|Reprocessing Scripts

Build your game to all targets
- web
- native macos
- native ios
- native android

Usage:
- rsb all
    builds all targets
- rsb js
    builds js target to `./public`
- rsb js:serve [port: default 3451]
    builds js target and starts a web server in `./public`
- rsb native
    builds macos native
- rsb native:hot
    builds native, runs the server, and starts the app in hot-reloading mode
- rsb native:bundle
    builds native and bundles
- rsb ios
    builds for ios
- rsb ios:full
    builds for ios & runs xcodebuild
- rsb ios:run
    builds for ios & runs xcodebuild & runs it in a simulator
- rsb android
    builds for android
- rsb android:full
    builds for android & runs `./gradlew assembleDebug`
- rsb android:run
    builds for android & runs `./gradlew installDebug` in the `./android` directory

|};

let main = (bsconfig) => switch (Sys.argv) {
| [|_, "all"|] => {Native.byte(); Native.run(); Android.both(); IOS.both(); Js.build()}
| [|_, "js"|] => Js.build()
| [|_, "js:serve"|] => {
    let (poll, _) = Js.watch();
    BuildUtils.showCommand("open http://localhost:3451") |> ignore;
    print_endline("Static server on http://localhost:3451");
    ReasonSimpleServer.Static.run(~poll, ~port=3451, "./public")
}
| [|_, "native"|] => Native.byte() |> ignore
| [|_, "native:hot"|] => Native.hot(bsconfig)
| [|_, "native:bundle"|] => {Native.run();Native.bundle(bsconfig)}
| [|_, "ios"|] => {IOS.both(); IOS.xcodebuild()}
| [|_, "ios:sim"|] => {IOS.both(); IOS.xcodebuild(); IOS.startSimulator()}
| [|_, "ios:device"|] => {IOS.both(); IOS.buildForDevice()}
| [|_, "android"|] => {Android.both(); Android.assemble() |> ignore}
| [|_, "android:run"|] => {Android.both(); Android.install() |> ignore; Android.run(bsconfig)}
| [|_, "android:hot"|] => {Android.hot(bsconfig)}
| _ => {print_endline(usage); exit(1)}
};

switch (ReasonCliTools.Files.readFile("bsconfig.json")) {
| None => {
    print_endline("No bsconfig.json file found in current directory");
    exit(1)
}
| Some(text) => {
    let json = try(Some(Json.parse(text))) { | _ => None };
    switch json {
    | None => {print_endline("Failed to parse bsconfig.json"); exit(1)}
    | Some(json) => main(json)
    }
}
};
/* main(ReasonCliTools.Files.readFile("bsconfig.json")) */