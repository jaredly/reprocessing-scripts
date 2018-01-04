
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
- rsb android
    builds for android
- rsb android:run
    builds for android & runs `./gradlew installDebug` in the `./android` directory

|};

let main = () => switch (Sys.argv) {
| [|_, "all"|] => {Native.byte(); Native.run(); Android.both(); IOS.build(); Js.build()}
| [|_, "js"|] => Js.build()
| [|_, "js:serve"|] => {
    let (poll, _) = Js.watch();
    BuildUtils.showCommand("open http://localhost:3451");
    StaticServer.run(~poll, "./public")
}
| [|_, "native"|] => Native.byte() |> ignore
| [|_, "native:hot"|] => Native.hot()
| [|_, "native:bundle"|] => {Native.run();Native.bundle()}
| [|_, "ios"|] => IOS.build()
| [|_, "android"|] => Android.both()
| [|_, "android:run"|] => {Android.both(); Android.install() |> ignore}
| _ => {print_endline(usage); exit(1)}
};

main()