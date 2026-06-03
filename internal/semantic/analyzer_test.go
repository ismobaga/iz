package semantic_test

import (
"strings"
"testing"

"github.com/ismobaga/iz/internal/lexer"
"github.com/ismobaga/iz/internal/parser"
"github.com/ismobaga/iz/internal/semantic"
)

func TestAnalyzeValidProgram(t *testing.T) {
source := `fn add(a: int, b: int) -> int {
    return a + b
}

fn main() -> int {
    let result = add(1, 2)
    if result == 3 {
        return result
    }
    return 0
}`

diagnostics := analyzeSource(t, source)
if len(diagnostics) > 0 {
t.Fatalf("unexpected diagnostics: %v", diagnostics)
}
}

func TestAnalyzeTypeMismatch(t *testing.T) {
source := `fn main() -> int {
    let ready: bool = 1
    return 0
}`

diagnostics := analyzeSource(t, source)
if len(diagnostics) != 1 {
t.Fatalf("expected one diagnostic, got %v", diagnostics)
}
if !strings.Contains(diagnostics[0].Error(), "cannot assign int to bool") {
t.Fatalf("unexpected diagnostic: %v", diagnostics[0])
}
}

func analyzeSource(t *testing.T, source string) []semantic.Diagnostic {
t.Helper()
tokens, err := lexer.Tokenize(source)
if err != nil {
t.Fatalf("tokenize: %v", err)
}
program, errs := parser.New(tokens).ParseProgram()
if len(errs) > 0 {
t.Fatalf("parse errors: %v", errs)
}
return semantic.New().Analyze(program)
}
