package lexer_test

import (
"bufio"
"fmt"
"os"
"path/filepath"
"strings"
"testing"

"github.com/ismobaga/iz/internal/lexer"
"github.com/ismobaga/iz/internal/token"
)

func TestLexerFixtures(t *testing.T) {
fixtures, err := filepath.Glob(filepath.Join("..", "..", "testdata", "lexer", "*.izi"))
if err != nil {
t.Fatalf("glob fixtures: %v", err)
}

for _, fixture := range fixtures {
t.Run(filepath.Base(fixture), func(t *testing.T) {
source, err := os.ReadFile(fixture)
if err != nil {
t.Fatalf("read fixture: %v", err)
}

tokens, err := lexer.Tokenize(string(source))
if err != nil {
t.Fatalf("tokenize: %v", err)
}

expected, err := os.ReadFile(strings.TrimSuffix(fixture, ".izi") + ".tokens")
if err != nil {
t.Fatalf("read expected: %v", err)
}

actualLines := make([]string, 0, len(tokens))
for _, tok := range tokens {
if tok.Type == token.EOF {
continue
}
actualLines = append(actualLines, fmt.Sprintf("%s|%v|%d:%d", tok.Type, tokenValue(tok), tok.Pos.Line, tok.Pos.Column))
}

expectedLines := scanLines(string(expected))
if strings.Join(actualLines, "\n") != strings.Join(expectedLines, "\n") {
t.Fatalf("tokens mismatch\nexpected:\n%s\nactual:\n%s", strings.Join(expectedLines, "\n"), strings.Join(actualLines, "\n"))
}
})
}
}

func tokenValue(tok token.Token) any {
if tok.Type == token.String && tok.Literal != nil {
return tok.Literal
}
return tok.Lexeme
}

func scanLines(input string) []string {
var lines []string
scanner := bufio.NewScanner(strings.NewReader(input))
for scanner.Scan() {
line := strings.TrimSpace(scanner.Text())
if line == "" {
continue
}
lines = append(lines, line)
}
return lines
}
