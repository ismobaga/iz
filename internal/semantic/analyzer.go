package semantic

import (
	"fmt"

	"github.com/ismobaga/iz/internal/ast"
	"github.com/ismobaga/iz/internal/token"
)

type Diagnostic struct {
	Pos     token.Position
	Message string
}

func (d Diagnostic) Error() string {
	return fmt.Sprintf("%s: %s", d.Pos, d.Message)
}

type functionSymbol struct {
	params  []string
	result  string
	builtin bool
}

type scope struct {
	parent *scope
	vars   map[string]string
}

func newScope(parent *scope) *scope {
	return &scope{parent: parent, vars: map[string]string{}}
}

func (s *scope) define(name, typeName string) bool {
	if _, exists := s.vars[name]; exists {
		return false
	}
	s.vars[name] = typeName
	return true
}

func (s *scope) lookup(name string) (string, bool) {
	if value, ok := s.vars[name]; ok {
		return value, true
	}
	if s.parent != nil {
		return s.parent.lookup(name)
	}
	return "", false
}

type Analyzer struct {
	functions     map[string]functionSymbol
	diagnostics   []Diagnostic
	currentReturn string
}

func New() *Analyzer {
	return &Analyzer{functions: map[string]functionSymbol{
		"print": {params: []string{"any"}, result: "void", builtin: true},
	}}
}

func (a *Analyzer) Analyze(program *ast.Program) []Diagnostic {
	a.diagnostics = nil
	a.declareFunctions(program)
	for _, fn := range program.Functions {
		a.analyzeFunction(fn)
	}
	return a.diagnostics
}

func (a *Analyzer) declareFunctions(program *ast.Program) {
	for _, fn := range program.Functions {
		if !isValidType(fn.ReturnType) {
			a.error(fn.Pos, "unknown return type %q", fn.ReturnType)
		}

		paramTypes := make([]string, 0, len(fn.Params))
		for _, param := range fn.Params {
			if !isValidType(param.Type) {
				a.error(fn.Pos, "unknown parameter type %q", param.Type)
			}
			paramTypes = append(paramTypes, param.Type)
		}

		if _, exists := a.functions[fn.Name]; exists {
			a.error(fn.Pos, "function %q already defined", fn.Name)
			continue
		}
		a.functions[fn.Name] = functionSymbol{params: paramTypes, result: fn.ReturnType}
	}
}

func (a *Analyzer) analyzeFunction(fn *ast.FunctionDecl) {
	fnScope := newScope(nil)
	for _, param := range fn.Params {
		if !fnScope.define(param.Name, param.Type) {
			a.error(fn.Pos, "parameter %q already defined", param.Name)
		}
	}

	previousReturn := a.currentReturn
	a.currentReturn = fn.ReturnType
	a.analyzeBlock(fn.Body, fnScope)
	a.currentReturn = previousReturn
}

func (a *Analyzer) analyzeBlock(block *ast.BlockStmt, parent *scope) {
	blockScope := newScope(parent)
	for _, stmt := range block.Statements {
		a.analyzeStatement(stmt, blockScope)
	}
}

func (a *Analyzer) analyzeStatement(stmt ast.Statement, current *scope) {
	switch node := stmt.(type) {
	case *ast.BlockStmt:
		a.analyzeBlock(node, current)
	case *ast.LetStmt:
		valueType := a.analyzeExpression(node.Value, current)
		declaredType := node.TypeName
		if declaredType == "" {
			declaredType = valueType
		}
		if declaredType == "" {
			a.error(node.Pos, "unable to infer type for %q", node.Name)
			return
		}
		if node.TypeName != "" && node.TypeName != valueType {
			a.error(node.Pos, "cannot assign %s to %s", valueType, node.TypeName)
		}
		if !current.define(node.Name, declaredType) {
			a.error(node.Pos, "variable %q already defined in this scope", node.Name)
		}
	case *ast.ReturnStmt:
		valueType := "void"
		if node.Value != nil {
			valueType = a.analyzeExpression(node.Value, current)
		}
		if valueType != a.currentReturn {
			a.error(node.Pos, "return type mismatch: expected %s, got %s", a.currentReturn, valueType)
		}
	case *ast.IfStmt:
		conditionType := a.analyzeExpression(node.Condition, current)
		if conditionType != "bool" {
			a.error(node.Pos, "if condition must be bool, got %s", conditionType)
		}
		a.analyzeBlock(node.Then, current)
		if node.Else != nil {
			a.analyzeBlock(node.Else, current)
		}
	case *ast.ExprStmt:
		a.analyzeExpression(node.Expr, current)
	}
}

func (a *Analyzer) analyzeExpression(expr ast.Expression, current *scope) string {
	switch node := expr.(type) {
	case *ast.IdentifierExpr:
		if valueType, ok := current.lookup(node.Name); ok {
			return valueType
		}
		if fn, ok := a.functions[node.Name]; ok {
			return fn.result
		}
		a.error(node.Pos, "undefined identifier %q", node.Name)
		return ""
	case *ast.IntegerLiteral:
		return "int"
	case *ast.StringLiteral:
		return "string"
	case *ast.BoolLiteral:
		return "bool"
	case *ast.UnaryExpr:
		rightType := a.analyzeExpression(node.Right, current)
		switch node.Operator {
		case "-":
			if rightType != "int" {
				a.error(node.Pos, "unary - expects int, got %s", rightType)
			}
			return "int"
		case "!":
			if rightType != "bool" {
				a.error(node.Pos, "unary ! expects bool, got %s", rightType)
			}
			return "bool"
		}
	case *ast.BinaryExpr:
		leftType := a.analyzeExpression(node.Left, current)
		rightType := a.analyzeExpression(node.Right, current)
		switch node.Operator {
		case "+", "-", "*", "/":
			if leftType != "int" || rightType != "int" {
				a.error(node.Pos, "operator %s expects int operands, got %s and %s", node.Operator, leftType, rightType)
			}
			return "int"
		case "<", "<=", ">", ">=":
			if leftType != "int" || rightType != "int" {
				a.error(node.Pos, "operator %s expects int operands, got %s and %s", node.Operator, leftType, rightType)
			}
			return "bool"
		case "==", "!=":
			if leftType == "" || rightType == "" {
				return "bool"
			}
			if leftType != rightType {
				a.error(node.Pos, "operator %s expects matching operand types, got %s and %s", node.Operator, leftType, rightType)
			}
			return "bool"
		}
	case *ast.CallExpr:
		ident, ok := node.Callee.(*ast.IdentifierExpr)
		if !ok {
			a.error(node.Pos, "call target must be a function name")
			return ""
		}
		fn, exists := a.functions[ident.Name]
		if !exists {
			a.error(node.Pos, "undefined function %q", ident.Name)
			return ""
		}
		if len(node.Arguments) != len(fn.params) {
			a.error(node.Pos, "function %q expects %d arguments, got %d", ident.Name, len(fn.params), len(node.Arguments))
			return fn.result
		}
		for index, argument := range node.Arguments {
			argumentType := a.analyzeExpression(argument, current)
			if fn.params[index] == "any" {
				continue
			}
			if argumentType != fn.params[index] {
				a.error(argument.Position(), "argument %d to %q must be %s, got %s", index+1, ident.Name, fn.params[index], argumentType)
			}
		}
		return fn.result
	}
	return ""
}

func isValidType(typeName string) bool {
	switch typeName {
	case "int", "bool", "string", "void":
		return true
	default:
		return false
	}
}

func (a *Analyzer) error(pos token.Position, format string, args ...any) {
	a.diagnostics = append(a.diagnostics, Diagnostic{Pos: pos, Message: fmt.Sprintf(format, args...)})
}
