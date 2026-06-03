package backend

import (
	"encoding/json"
	"os"
	"path/filepath"

	"github.com/ismobaga/iz/internal/ast"
)

func WriteAST(program *ast.Program, path string) error {
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}

	payload, err := json.MarshalIndent(program, "", "  ")
	if err != nil {
		return err
	}

	return os.WriteFile(path, append(payload, '\n'), 0o644)
}
