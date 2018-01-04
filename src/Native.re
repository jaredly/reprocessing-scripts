
let byte = () => BuildUtils.showCommand("./node_modules/.bin/bsb -make-world -backend bytecode");
let run = () => BuildUtils.showCommand("./node_modules/.bin/bsb -make-world -backend native");

let hot = () => {
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
  /** TODO imple */
  ()
};