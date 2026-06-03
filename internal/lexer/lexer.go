package lexer

import (
	"fmt"
	"strconv"
	"strings"
	"unicode"

	"github.com/ismobaga/iz/internal/token"
)

type Lexer struct {
	input  []rune
	index  int
	line   int
	column int
}

func New(input string) *Lexer {
	return &Lexer{input: []rune(input), line: 1, column: 1}
}

func Tokenize(input string) ([]token.Token, error) {
	lexer := New(input)
	var tokens []token.Token
	for {
		tok, err := lexer.NextToken()
		if err != nil {
			return nil, err
		}
		tokens = append(tokens, tok)
		if tok.Type == token.EOF {
			return tokens, nil
		}
	}
}

func (l *Lexer) NextToken() (token.Token, error) {
	l.skipWhitespaceAndComments()

	pos := token.Position{Offset: l.index, Line: l.line, Column: l.column}
	if l.atEnd() {
		return token.Token{Type: token.EOF, Pos: pos}, nil
	}

	ch := l.peek()
	switch ch {
	case '(':
		l.advance()
		return token.Token{Type: token.LParen, Lexeme: "(", Pos: pos}, nil
	case ')':
		l.advance()
		return token.Token{Type: token.RParen, Lexeme: ")", Pos: pos}, nil
	case '{':
		l.advance()
		return token.Token{Type: token.LBrace, Lexeme: "{", Pos: pos}, nil
	case '}':
		l.advance()
		return token.Token{Type: token.RBrace, Lexeme: "}", Pos: pos}, nil
	case ',':
		l.advance()
		return token.Token{Type: token.Comma, Lexeme: ",", Pos: pos}, nil
	case ':':
		l.advance()
		return token.Token{Type: token.Colon, Lexeme: ":", Pos: pos}, nil
	case ';':
		l.advance()
		return token.Token{Type: token.Semicolon, Lexeme: ";", Pos: pos}, nil
	case '+':
		l.advance()
		return token.Token{Type: token.Plus, Lexeme: "+", Pos: pos}, nil
	case '*':
		l.advance()
		return token.Token{Type: token.Star, Lexeme: "*", Pos: pos}, nil
	case '/':
		l.advance()
		return token.Token{Type: token.Slash, Lexeme: "/", Pos: pos}, nil
	case '-':
		l.advance()
		if l.match('>') {
			return token.Token{Type: token.Arrow, Lexeme: "->", Pos: pos}, nil
		}
		return token.Token{Type: token.Minus, Lexeme: "-", Pos: pos}, nil
	case '!':
		l.advance()
		if l.match('=') {
			return token.Token{Type: token.NotEqual, Lexeme: "!=", Pos: pos}, nil
		}
		return token.Token{Type: token.Bang, Lexeme: "!", Pos: pos}, nil
	case '=':
		l.advance()
		if l.match('=') {
			return token.Token{Type: token.Equal, Lexeme: "==", Pos: pos}, nil
		}
		return token.Token{Type: token.Assign, Lexeme: "=", Pos: pos}, nil
	case '<':
		l.advance()
		if l.match('=') {
			return token.Token{Type: token.LessEqual, Lexeme: "<=", Pos: pos}, nil
		}
		return token.Token{Type: token.Less, Lexeme: "<", Pos: pos}, nil
	case '>':
		l.advance()
		if l.match('=') {
			return token.Token{Type: token.GreaterEqual, Lexeme: ">=", Pos: pos}, nil
		}
		return token.Token{Type: token.Greater, Lexeme: ">", Pos: pos}, nil
	case '"':
		literal, raw, err := l.readString()
		if err != nil {
			return token.Token{}, err
		}
		return token.Token{Type: token.String, Lexeme: raw, Literal: literal, Pos: pos}, nil
	}

	if unicode.IsLetter(ch) || ch == '_' {
		lexeme := l.readIdentifier()
		tokenType := token.LookupIdentifier(lexeme)
		literal := any(nil)
		if tokenType == token.True {
			literal = true
		}
		if tokenType == token.False {
			literal = false
		}
		return token.Token{Type: tokenType, Lexeme: lexeme, Literal: literal, Pos: pos}, nil
	}

	if unicode.IsDigit(ch) {
		lexeme := l.readNumber()
		value, _ := strconv.ParseInt(lexeme, 10, 64)
		return token.Token{Type: token.Int, Lexeme: lexeme, Literal: value, Pos: pos}, nil
	}

	l.advance()
	return token.Token{}, fmt.Errorf("%s: unexpected character %q", pos, ch)
}

func (l *Lexer) skipWhitespaceAndComments() {
	for !l.atEnd() {
		if unicode.IsSpace(l.peek()) {
			l.advance()
			continue
		}
		if l.peek() == '/' && l.peekNext() == '/' {
			for !l.atEnd() && l.peek() != '\n' {
				l.advance()
			}
			continue
		}
		break
	}
}

func (l *Lexer) readIdentifier() string {
	start := l.index
	for !l.atEnd() {
		ch := l.peek()
		if !unicode.IsLetter(ch) && !unicode.IsDigit(ch) && ch != '_' {
			break
		}
		l.advance()
	}
	return string(l.input[start:l.index])
}

func (l *Lexer) readNumber() string {
	start := l.index
	for !l.atEnd() && unicode.IsDigit(l.peek()) {
		l.advance()
	}
	return string(l.input[start:l.index])
}

func (l *Lexer) readString() (string, string, error) {
	start := l.index
	l.advance()
	var builder strings.Builder
	for !l.atEnd() {
		ch := l.peek()
		if ch == '"' {
			l.advance()
			return builder.String(), string(l.input[start:l.index]), nil
		}
		if ch == '\\' {
			l.advance()
			if l.atEnd() {
				break
			}
			escaped := l.advance()
			switch escaped {
			case 'n':
				builder.WriteRune('\n')
			case 't':
				builder.WriteRune('\t')
			case 'r':
				builder.WriteRune('\r')
			case '"':
				builder.WriteRune('"')
			case '\\':
				builder.WriteRune('\\')
			default:
				return "", "", fmt.Errorf("%d:%d: unsupported escape sequence \\%c", l.line, l.column, escaped)
			}
			continue
		}
		builder.WriteRune(l.advance())
	}
	return "", "", fmt.Errorf("%d:%d: unterminated string literal", l.line, l.column)
}

func (l *Lexer) atEnd() bool {
	return l.index >= len(l.input)
}

func (l *Lexer) peek() rune {
	if l.atEnd() {
		return 0
	}
	return l.input[l.index]
}

func (l *Lexer) peekNext() rune {
	if l.index+1 >= len(l.input) {
		return 0
	}
	return l.input[l.index+1]
}

func (l *Lexer) match(expected rune) bool {
	if l.atEnd() || l.peek() != expected {
		return false
	}
	l.advance()
	return true
}

func (l *Lexer) advance() rune {
	ch := l.input[l.index]
	l.index++
	if ch == '\n' {
		l.line++
		l.column = 1
	} else {
		l.column++
	}
	return ch
}
