
let run = () => BuildUtils.showCommand("./node_modules/.bin/bsb -make-world -backend bytecode");

let hot = () => {
  run() |> ignore;
  let (cmd, closecmd, _) = BuildUtils.pollableCommand("Bucklescript", "./node_modules/.bin/bsb -make-world -backend bytecode -w");
  let (poll, close) = BuildUtils.keepAlive("Game", "bash", [|"-c", "-c", "./lib/bs/bytecode/indexhot.byte"|]);
  let lastCheck = ref(Unix.gettimeofday());
  while (true) {
    cmd();
    poll();
  };
};