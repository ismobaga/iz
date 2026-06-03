package parser

import (
	"fmt"

	"github.com/ismobaga/iz/internal/ast"
	"github.com/ismobaga/iz/internal/token"
)

type Parser struct {
	tokens []token.Token
	index  int
	errors []string
}

func New(tokens []token.Token) *Parser {
	return &Parser{tokens: tokens}
}

func (p *Parser) ParseProgram() (*ast.Program, []string) {
	program := &ast.Program{Kind: "program", Functions: []*ast.FunctionDecl{}}
	for !p.isAtEnd() {
		if p.check(token.EOF) {
			break
		}
		fn := p.parseFunction()
		if fn == nil {
			p.synchronize()
			continue
		}
		program.Functions = append(program.Functions, fn)
	}
	return program, p.errors
}

func (p *Parser) parseFunction() *ast.FunctionDecl {
	if !p.match(token.Fn) {
		p.errorAtCurrent("expected 'fn' at top level")
		return nil
	}

	fnToken := p.previous()
	name := p.consume(token.Ident, "expected function name")
	p.consume(token.LParen, "expected '('")

	params := make([]ast.Parameter, 0)
	if !p.check(token.RParen) {
		for {
			paramName := p.consume(token.Ident, "expected parameter name")
			p.consume(token.Colon, "expected ':' after parameter name")
			paramType := p.consumeTypeName()
			params = append(params, ast.Parameter{Name: paramName.Lexeme, Type: paramType})
			if !p.match(token.Comma) {
				break
			}
		}
	}

	p.consume(token.RParen, "expected ')' after parameters")
	returnType := "void"
	if p.match(token.Arrow) {
		returnType = p.consumeTypeName()
	}

	body := p.parseBlock()
	if body == nil {
		return nil
	}

	return &ast.FunctionDecl{
		Kind:       "function",
		Name:       name.Lexeme,
		Params:     params,
		ReturnType: returnType,
		Body:       body,
		Pos:        fnToken.Pos,
	}
}

func (p *Parser) parseBlock() *ast.BlockStmt {
	lbrace := p.consume(token.LBrace, "expected '{'")
	if lbrace.Type == token.Illegal {
		return nil
	}

	statements := make([]ast.Statement, 0)
	for !p.check(token.RBrace) && !p.check(token.EOF) {
		stmt := p.parseStatement()
		if stmt == nil {
			p.synchronize()
			continue
		}
		statements = append(statements, stmt)
	}

	p.consume(token.RBrace, "expected '}' after block")
	return &ast.BlockStmt{Kind: "block", Statements: statements, Pos: lbrace.Pos}
}

func (p *Parser) parseStatement() ast.Statement {
	switch {
	case p.match(token.Let):
		return p.parseLetStatement(p.previous())
	case p.match(token.Return):
		return p.parseReturnStatement(p.previous())
	case p.match(token.If):
		return p.parseIfStatement(p.previous())
	default:
		return p.parseExpressionStatement()
	}
}

func (p *Parser) parseLetStatement(start token.Token) ast.Statement {
	name := p.consume(token.Ident, "expected variable name")
	typeName := ""
	if p.match(token.Colon) {
		typeName = p.consumeTypeName()
	}
	p.consume(token.Assign, "expected '=' in variable declaration")
	value := p.parseExpression()
	p.match(token.Semicolon)
	return &ast.LetStmt{Kind: "let", Name: name.Lexeme, TypeName: typeName, Value: value, Pos: start.Pos}
}

func (p *Parser) parseReturnStatement(start token.Token) ast.Statement {
	if p.check(token.RBrace) || p.check(token.Semicolon) {
		p.match(token.Semicolon)
		return &ast.ReturnStmt{Kind: "return", Pos: start.Pos}
	}
	value := p.parseExpression()
	p.match(token.Semicolon)
	return &ast.ReturnStmt{Kind: "return", Value: value, Pos: start.Pos}
}

func (p *Parser) parseIfStatement(start token.Token) ast.Statement {
	condition := p.parseExpression()
	thenBlock := p.parseBlock()
	if thenBlock == nil {
		return nil
	}
	var elseBlock *ast.BlockStmt
	if p.match(token.Else) {
		elseBlock = p.parseBlock()
	}
	return &ast.IfStmt{Kind: "if", Condition: condition, Then: thenBlock, Else: elseBlock, Pos: start.Pos}
}

func (p *Parser) parseExpressionStatement() ast.Statement {
	expr := p.parseExpression()
	p.match(token.Semicolon)
	return &ast.ExprStmt{Kind: "expr", Expr: expr, Pos: expr.Position()}
}

func (p *Parser) parseExpression() ast.Expression {
	return p.parseEquality()
}

func (p *Parser) parseEquality() ast.Expression {
	expr := p.parseComparison()
	for p.match(token.Equal, token.NotEqual) {
		operator := p.previous()
		right := p.parseComparison()
		expr = &ast.BinaryExpr{Kind: "binary", Operator: operator.Lexeme, Left: expr, Right: right, Pos: operator.Pos}
	}
	return expr
}

func (p *Parser) parseComparison() ast.Expression {
	expr := p.parseTerm()
	for p.match(token.Less, token.LessEqual, token.Greater, token.GreaterEqual) {
		operator := p.previous()
		right := p.parseTerm()
		expr = &ast.BinaryExpr{Kind: "binary", Operator: operator.Lexeme, Left: expr, Right: right, Pos: operator.Pos}
	}
	return expr
}

func (p *Parser) parseTerm() ast.Expression {
	expr := p.parseFactor()
	for p.match(token.Plus, token.Minus) {
		operator := p.previous()
		right := p.parseFactor()
		expr = &ast.BinaryExpr{Kind: "binary", Operator: operator.Lexeme, Left: expr, Right: right, Pos: operator.Pos}
	}
	return expr
}

func (p *Parser) parseFactor() ast.Expression {
	expr := p.parseUnary()
	for p.match(token.Star, token.Slash) {
		operator := p.previous()
		right := p.parseUnary()
		expr = &ast.BinaryExpr{Kind: "binary", Operator: operator.Lexeme, Left: expr, Right: right, Pos: operator.Pos}
	}
	return expr
}

func (p *Parser) parseUnary() ast.Expression {
	if p.match(token.Bang, token.Minus) {
		operator := p.previous()
		right := p.parseUnary()
		return &ast.UnaryExpr{Kind: "unary", Operator: operator.Lexeme, Right: right, Pos: operator.Pos}
	}
	return p.parseCall()
}

func (p *Parser) parseCall() ast.Expression {
	expr := p.parsePrimary()
	for {
		if !p.match(token.LParen) {
			break
		}
		open := p.previous()
		arguments := make([]ast.Expression, 0)
		if !p.check(token.RParen) {
			for {
				arguments = append(arguments, p.parseExpression())
				if !p.match(token.Comma) {
					break
				}
			}
		}
		p.consume(token.RParen, "expected ')' after arguments")
		expr = &ast.CallExpr{Kind: "call", Callee: expr, Arguments: arguments, Pos: open.Pos}
	}
	return expr
}

func (p *Parser) parsePrimary() ast.Expression {
	if p.match(token.Int) {
		tok := p.previous()
		return &ast.IntegerLiteral{Kind: "int", Value: tok.Literal.(int64), Pos: tok.Pos}
	}
	if p.match(token.String) {
		tok := p.previous()
		return &ast.StringLiteral{Kind: "string", Value: tok.Literal.(string), Pos: tok.Pos}
	}
	if p.match(token.True) {
		tok := p.previous()
		return &ast.BoolLiteral{Kind: "bool", Value: true, Pos: tok.Pos}
	}
	if p.match(token.False) {
		tok := p.previous()
		return &ast.BoolLiteral{Kind: "bool", Value: false, Pos: tok.Pos}
	}
	if p.match(token.Ident) {
		tok := p.previous()
		return &ast.IdentifierExpr{Kind: "identifier", Name: tok.Lexeme, Pos: tok.Pos}
	}
	if p.match(token.LParen) {
		expr := p.parseExpression()
		p.consume(token.RParen, "expected ')' after expression")
		return expr
	}

	p.errorAtCurrent("expected expression")
	fallback := p.peek()
	return &ast.IdentifierExpr{Kind: "identifier", Name: "<error>", Pos: fallback.Pos}
}

func (p *Parser) consumeTypeName() string {
	tok := p.consume(token.Ident, "expected type name")
	return tok.Lexeme
}

func (p *Parser) match(types ...token.Type) bool {
	for _, expected := range types {
		if p.check(expected) {
			p.advance()
			return true
		}
	}
	return false
}

func (p *Parser) consume(expected token.Type, message string) token.Token {
	if p.check(expected) {
		return p.advance()
	}
	p.errorAtCurrent(message)
	return token.Token{Type: token.Illegal, Pos: p.peek().Pos}
}

func (p *Parser) check(expected token.Type) bool {
	if p.isAtEnd() {
		return expected == token.EOF
	}
	return p.peek().Type == expected
}

func (p *Parser) advance() token.Token {
	if !p.isAtEnd() {
		p.index++
	}
	return p.previous()
}

func (p *Parser) isAtEnd() bool {
	return p.peek().Type == token.EOF
}

func (p *Parser) peek() token.Token {
	return p.tokens[p.index]
}

func (p *Parser) previous() token.Token {
	return p.tokens[p.index-1]
}

func (p *Parser) errorAtCurrent(message string) {
	current := p.peek()
	p.errors = append(p.errors, fmt.Sprintf("%s: %s", current.Pos, message))
}

func (p *Parser) synchronize() {
	for !p.isAtEnd() {
		if p.previous().Type == token.Semicolon {
			return
		}
		switch p.peek().Type {
		case token.Fn, token.Let, token.Return, token.If:
			return
		default:
			p.advance()
		}
	}
}
