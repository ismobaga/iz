package driver

import (
"fmt"
"os"

"github.com/ismobaga/iz/internal/ast"
"github.com/ismobaga/iz/internal/lexer"
"github.com/ismobaga/iz/internal/parser"
"github.com/ismobaga/iz/internal/semantic"
)

func LoadProgram(path string) (*ast.Program, []string, error) {
source, err := os.ReadFile(path)
if err != nil {
return nil, nil, fmt.Errorf("read %s: %w", path, err)
}

tokens, lexErr := lexer.Tokenize(string(source))
if lexErr != nil {
return nil, []string{lexErr.Error()}, nil
}

parsedProgram, parseErrs := parser.New(tokens).ParseProgram()
if len(parseErrs) > 0 {
return parsedProgram, parseErrs, nil
}

semanticErrs := semantic.New().Analyze(parsedProgram)
if len(semanticErrs) > 0 {
return parsedProgram, diagnosticsFromSemantic(semanticErrs), nil
}

return parsedProgram, nil, nil
}

func diagnosticsFromSemantic(errors []semantic.Diagnostic) []string {
result := make([]string, 0, len(errors))
for _, err := range errors {
result = append(result, err.Error())
}
return result
}
