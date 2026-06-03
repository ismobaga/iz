package token

import "fmt"

type Type string

type Position struct {
Offset int `json:"offset"`
Line   int `json:"line"`
Column int `json:"column"`
}

func (p Position) String() string {
return fmt.Sprintf("%d:%d", p.Line, p.Column)
}

type Token struct {
Type    Type     `json:"type"`
Lexeme  string   `json:"lexeme"`
Literal any      `json:"literal,omitempty"`
Pos     Position `json:"position"`
}

const (
Illegal Type = "ILLEGAL"
EOF     Type = "EOF"

Ident  Type = "IDENT"
Int    Type = "INT"
String Type = "STRING"

Assign Type = "ASSIGN"
Plus   Type = "PLUS"
Minus  Type = "MINUS"
Star   Type = "STAR"
Slash  Type = "SLASH"
Bang   Type = "BANG"

Equal        Type = "EQ"
NotEqual     Type = "NOT_EQ"
Less         Type = "LT"
LessEqual    Type = "LT_EQ"
Greater      Type = "GT"
GreaterEqual Type = "GT_EQ"
Arrow        Type = "ARROW"

Comma     Type = "COMMA"
Colon     Type = "COLON"
Semicolon Type = "SEMICOLON"
LParen    Type = "LPAREN"
RParen    Type = "RPAREN"
LBrace    Type = "LBRACE"
RBrace    Type = "RBRACE"

Fn     Type = "FN"
Let    Type = "LET"
Return Type = "RETURN"
If     Type = "IF"
Else   Type = "ELSE"
True   Type = "TRUE"
False  Type = "FALSE"
)

var keywords = map[string]Type{
"fn":     Fn,
"let":    Let,
"return": Return,
"if":     If,
"else":   Else,
"true":   True,
"false":  False,
}

func LookupIdentifier(identifier string) Type {
if tokenType, ok := keywords[identifier]; ok {
return tokenType
}
return Ident
}
