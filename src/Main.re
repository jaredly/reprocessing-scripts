
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

open Commands;
let main = () => switch (Sys.argv) {
| [|_, "all"|] => {native(); android(); ios(); js()}
| [|_, "js"|] => Js.build()
| [|_, "js:serve"|] => {
    let (poll, _) = Js.watch();
    StaticServer.run(~poll, "./public")
}
| [|_, "native"|] => native()
| [|_, "native:hot"|] => nativeHot()
| [|_, "native:bundle"|] => {native();bundleApp()}
| [|_, "ios"|] => ios()
| [|_, "android"|] => android()
| [|_, "android:run"|] => {android(); gradleInstall()}
| _ => {print_endline(usage); exit(1)}
};

main()