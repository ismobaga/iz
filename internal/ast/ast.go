package ast

import "github.com/ismobaga/iz/internal/token"

type Program struct {
Kind      string          `json:"kind"`
Functions []*FunctionDecl `json:"functions"`
}

type FunctionDecl struct {
Kind       string         `json:"kind"`
Name       string         `json:"name"`
Params     []Parameter    `json:"params"`
ReturnType string         `json:"returnType"`
Body       *BlockStmt     `json:"body"`
Pos        token.Position `json:"position"`
}

type Parameter struct {
Name string `json:"name"`
Type string `json:"type"`
}

type Statement interface {
stmtNode()
Position() token.Position
}

type Expression interface {
exprNode()
Position() token.Position
}

type BlockStmt struct {
Kind       string         `json:"kind"`
Statements []Statement    `json:"statements"`
Pos        token.Position `json:"position"`
}

func (b *BlockStmt) stmtNode() {}
func (b *BlockStmt) Position() token.Position { return b.Pos }

type LetStmt struct {
Kind     string         `json:"kind"`
Name     string         `json:"name"`
TypeName string         `json:"typeName,omitempty"`
Value    Expression     `json:"value"`
Pos      token.Position `json:"position"`
}

func (s *LetStmt) stmtNode() {}
func (s *LetStmt) Position() token.Position { return s.Pos }

type ReturnStmt struct {
Kind  string         `json:"kind"`
Value Expression     `json:"value,omitempty"`
Pos   token.Position `json:"position"`
}

func (s *ReturnStmt) stmtNode() {}
func (s *ReturnStmt) Position() token.Position { return s.Pos }

type ExprStmt struct {
Kind string         `json:"kind"`
Expr Expression     `json:"expr"`
Pos  token.Position `json:"position"`
}

func (s *ExprStmt) stmtNode() {}
func (s *ExprStmt) Position() token.Position { return s.Pos }

type IfStmt struct {
Kind      string         `json:"kind"`
Condition Expression     `json:"condition"`
Then      *BlockStmt     `json:"then"`
Else      *BlockStmt     `json:"else,omitempty"`
Pos       token.Position `json:"position"`
}

func (s *IfStmt) stmtNode() {}
func (s *IfStmt) Position() token.Position { return s.Pos }

type IdentifierExpr struct {
Kind string         `json:"kind"`
Name string         `json:"name"`
Pos  token.Position `json:"position"`
}

func (e *IdentifierExpr) exprNode() {}
func (e *IdentifierExpr) Position() token.Position { return e.Pos }

type IntegerLiteral struct {
Kind  string         `json:"kind"`
Value int64          `json:"value"`
Pos   token.Position `json:"position"`
}

func (e *IntegerLiteral) exprNode() {}
func (e *IntegerLiteral) Position() token.Position { return e.Pos }

type StringLiteral struct {
Kind  string         `json:"kind"`
Value string         `json:"value"`
Pos   token.Position `json:"position"`
}

func (e *StringLiteral) exprNode() {}
func (e *StringLiteral) Position() token.Position { return e.Pos }

type BoolLiteral struct {
Kind  string         `json:"kind"`
Value bool           `json:"value"`
Pos   token.Position `json:"position"`
}

func (e *BoolLiteral) exprNode() {}
func (e *BoolLiteral) Position() token.Position { return e.Pos }

type UnaryExpr struct {
Kind     string         `json:"kind"`
Operator string         `json:"operator"`
Right    Expression     `json:"right"`
Pos      token.Position `json:"position"`
}

func (e *UnaryExpr) exprNode() {}
func (e *UnaryExpr) Position() token.Position { return e.Pos }

type BinaryExpr struct {
Kind     string         `json:"kind"`
Operator string         `json:"operator"`
Left     Expression     `json:"left"`
Right    Expression     `json:"right"`
Pos      token.Position `json:"position"`
}

func (e *BinaryExpr) exprNode() {}
func (e *BinaryExpr) Position() token.Position { return e.Pos }

type CallExpr struct {
Kind      string         `json:"kind"`
Callee    Expression     `json:"callee"`
Arguments []Expression   `json:"arguments"`
Pos       token.Position `json:"position"`
}

func (e *CallExpr) exprNode() {}
func (e *CallExpr) Position() token.Position { return e.Pos }
