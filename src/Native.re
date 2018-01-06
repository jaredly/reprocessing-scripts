
let byte = () => BuildUtils.showCommand("./node_modules/.bin/bsb -make-world -backend bytecode");
let run = () => BuildUtils.showCommand("./node_modules/.bin/bsb -make-world -backend native");

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

let bundle = () => {
  print_endline("Not yet impl");
  /** TODO imple */
  ()
};