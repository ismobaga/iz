package parser_test

import (
"encoding/json"
"strings"
"testing"

"github.com/ismobaga/iz/internal/lexer"
"github.com/ismobaga/iz/internal/parser"
)

func TestParseProgramDump(t *testing.T) {
source := `fn add(a: int, b: int) -> int {
    let total = a + b
    return total
}

fn main() -> int {
    if add(20, 22) == 42 {
        return 1
    }
    return 0
}`

tokens, err := lexer.Tokenize(source)
if err != nil {
t.Fatalf("tokenize: %v", err)
}

program, errs := parser.New(tokens).ParseProgram()
if len(errs) > 0 {
t.Fatalf("parse errors: %v", errs)
}

payload, err := json.Marshal(program)
if err != nil {
t.Fatalf("marshal ast: %v", err)
}

dump := string(payload)
for _, fragment := range []string{"\"name\":\"add\"", "\"kind\":\"if\"", "\"operator\":\"==\""} {
if !strings.Contains(dump, fragment) {
t.Fatalf("ast dump missing %s in %s", fragment, dump)
}
}
}
