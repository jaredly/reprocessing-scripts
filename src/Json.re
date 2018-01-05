
type t =
  | String(string)
  | Number(float)
  | Array(list(t))
  | Object(list((string, t)))
  | True
  | False
  | Null;

let rec skipToNewline = (text, pos) => {
  if (pos >= String.length(text)) {
    pos
  } else if (text.[pos] == '\n') {
    pos + 1
  } else {
    skipToNewline(text, pos + 1)
  }
};

let rec skipWhite = (text, pos) => {
  if (text.[pos] == ' ' || text.[pos] == '\t' || text.[pos] == '\n') {
    skipWhite(text, pos + 1)
  } else {
    pos
  }
};

let parseString = (text, pos) => {
  let i = ref(pos);
  while (text.[i^] != '"') {
    i := i^ + (text.[i^] == '\\' ? 2 : 1)
  };
  /* print_endline(text ++ string_of_int(pos)); */
  (Scanf.unescaped(String.sub(text, pos, i^ - pos)), i^ + 1)
};

let parseNumber = (text, pos) => {
  let i = ref(pos);
  let len = String.length(text);
  while (i^ < len && Char.code('0') <= Char.code(text.[i^]) && Char.code(text.[i^]) <= Char.code('9')) {
    i := i^ + 1;
    /* print_endline(">" ++ string_of_int(pos) ++ " : " ++ string_of_int(i^)); */
  };
  let s = String.sub(text, pos, i^ - pos);
  /* print_endline(s); */
  (Number(float_of_string(s)), i^)
};

let expect = (char, text, pos) => {
  if (text.[pos] != char) {
    failwith("Expected something else")
  } else {
    pos + 1
  }
};

let parseComment: 'a . (string, int, (string, int) => 'a) => 'a = (text, pos, next) => {
  if (text.[pos] != '/') {
    failwith("Invalid syntax")
  } else {
    next(text, skipToNewline(text, pos + 1))
  }
};

let rec parse = (text, pos) => {
  if (pos >= String.length(text)) {
    failwith("Reached end of file without being done parsing")
  } else {
    switch (text.[pos]) {
    | '/' => parseComment(text, pos + 1, parse)
    | '[' => parseArray(text, pos + 1)
    | '{' => parseObject(text, pos + 1)
    | ' ' => parse(text, skipWhite(text, pos))
    | '"' => {
      let (s, pos) = parseString(text, pos + 1);
      (String(s), pos)
    }
    | '0'..'9' => parseNumber(text, pos)
    | _ => failwith("unexpected character")
    }
  }
}

and parseArrayInner = (text, pos) => {
  switch (text.[pos]) {
  | ' ' => parseArrayInner(text, skipWhite(text, pos))
  | ']' => ([], pos + 1)
  | '/' => parseComment(text, pos + 1, parseArrayInner)
  | ',' => {
    let (item, pos) = parse(text, pos + 1);
    let (rest, pos) = parseArrayInner(text, pos);
    ([item, ...rest], pos)
  }
  | _ => failwith("Unexpected character in array")
  }
}

and parseArray = (text, pos) => {
  switch (text.[pos]) {
  | ' ' => parseArray(text, skipWhite(text, pos))
  | ']' => (Array([]), pos + 1)
  | '/' => parseComment(text, pos + 1, parseArray)
  | _ => {
    let (item, pos) = parse(text, pos);
    let (rest, pos) = parseArrayInner(text, pos);
    (Array([item, ...rest]), pos)
  }
  }
}

and parseObjectValue = (text, pos) => {
  switch (text.[pos]) {
  | ' ' => parseObjectValue(text, skipWhite(text, pos))
  | '/' => parseComment(text, pos + 1, parseObjectValue)
  | '"' => {
    let (k, pos) = parseString(text, pos + 1);
    let pos = expect(':', text, skipWhite(text, pos));
    let (v, pos) = parse(text, pos);
    let (rest, pos) = parseObjectInner(text, pos);
    ([(k, v), ...rest], pos)
  }
  | _ => failwith("Unexpected character in object")
  }
}

and parseObjectInner = (text, pos) => {
  switch (text.[pos]) {
  | ' ' => parseObjectInner(text, skipWhite(text, pos))
  | '/' => parseComment(text, pos + 1, parseObjectInner)
  | '}' => ([], pos + 1)
  | ',' => {
    parseObjectValue(text, pos + 1)
  }
  | _ => failwith("Unexpected character in object")
  }
}

and parseObject = (text, pos) => {
  switch (text.[pos]) {
  | ' ' => parseObject(text, skipWhite(text, pos))
  | '}' => (Object([]), pos + 1)
  | '/' => parseComment(text, pos + 1, parseObject)
  | _ => {
    let (items, pos) = parseObjectValue(text, pos);
    (Object(items), pos)
  }
  }
}

;

let parse = text => {
  let (item, _) = parse(text, 0);
  item
};