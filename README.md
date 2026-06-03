# izi

izi is a simple, fast, AI-ready programming language.

## Vision

izi aims to combine:

- Go-like simplicity
- Python-like productivity
- C++/Rust-like performance
- Native AI tools

## Bootstrap decisions

The compiler bootstrap is intentionally small and practical:

- **Implementation language:** Go
- **Initial targets:** Linux, macOS, and Windows on amd64/arm64 through a portable Go CLI while the language frontend stabilizes
- **v0.1 feature set:** variables, primitive types (`int`, `bool`, `string`), basic expressions, functions, blocks, function calls, and `if/else`
- **Current output:** parse, type-check, and AST dump generation before native code generation lands

## Primary use cases

The language is being designed for:

- backend servers
- CLI tools
- scripting
- data processing
- machine learning and AI
- high-performance modules
- games or systems programming later

## Compiler architecture

The long-term compilation pipeline is:

```text
izi source code
   ↓
Lexer
   ↓
Parser
   ↓
AST
   ↓
Type checker
   ↓
IR
   ↓
LLVM IR
   ↓
Native binary
```

The current repository implements the first frontend slice:

```text
izi source code
   ↓
Lexer
   ↓
Parser
   ↓
AST
   ↓
Semantic/type checks
   ↓
AST dump artifact
```

## Repository layout

```text
cmd/izi/            CLI entrypoint
internal/token/     token definitions and source positions
internal/lexer/     lexer implementation and tests
internal/ast/       AST nodes
internal/parser/    parser and AST construction
internal/semantic/  scope and type analysis
internal/backend/   output artifact boundary
internal/driver/    end-to-end frontend pipeline
examples/           sample .izi programs
testdata/lexer/     lexer fixtures
```

## CLI workflow

```bash
go run ./cmd/izi run ./examples/hello.izi
go run ./cmd/izi build ./examples/hello.izi
```

- `izi run` validates input and prints the AST as JSON
- `izi build` validates input and writes a `build/*.ast.json` artifact

## Implemented syntax slice

```izi
fn add(a: int, b: int) -> int {
    return a + b
}

fn main() -> int {
    let result = add(20, 22)
    if result == 42 {
        return result
    }
    return 0
}
```

Supported today:

- function declarations
- typed parameters and return types
- `let` bindings with optional type annotations
- integer, boolean, and string literals
- unary and binary expressions
- function calls
- `if/else` blocks
- `return`

Planned next:

- loops
- arrays
- structs
- modules
- IR lowering and LLVM backend

## Quality tooling

```bash
make fmt
make build
make test
```

CI runs formatting, build, and tests on every push and pull request.

## Roadmap

### Phase 1 — Core language

#### Milestone 1: bootstrap frontend

- [x] compiler CLI skeleton
- [x] lexer with token fixtures and source locations
- [x] parser for functions, expressions, declarations, and blocks
- [x] semantic checks for scope and primitive types
- [x] AST dump output for `run` and `build`

#### Milestone 2: control flow and collections

Acceptance criteria:

- loops parse and type-check cleanly
- arrays and indexing work in the frontend
- parser coverage includes nested control flow and precedence edge cases

Deliverables:

- [ ] `while` / `for`
- [ ] arrays and indexing
- [ ] assignment and mutation
- [ ] richer diagnostics

#### Milestone 3: user-defined types and modules

Acceptance criteria:

- structs and methods resolve across files
- imports load module graphs deterministically
- semantic analysis understands named types

Deliverables:

- [ ] structs
- [ ] methods
- [ ] modules/imports
- [ ] multi-file compilation

#### Milestone 4: backend handoff

Acceptance criteria:

- typed frontend lowers to a stable IR
- `izi build` produces executable backend artifacts for a narrow target set
- sample programs compile end-to-end

Deliverables:

- [ ] IR design
- [ ] LLVM lowering
- [ ] native code generation
- [ ] executable smoke tests

### Phase 2 — Standard library

- file system
- strings
- math
- HTTP
- JSON
- CLI

### Phase 3 — AI support

- tensor type
- matrix operations
- automatic differentiation
- neural network layers
- GPU support
- Python/PyTorch interop first

### Phase 4 — Native AI framework

- izi-native tensor engine
- CUDA/Vulkan/Metal backend
- training and inference support

## Early implementation strategy

The first version focuses on a reliable frontend and small compiler pipeline before native code generation. LLVM remains the intended backend, but the current priority is making the surface language consistent, testable, and easy to evolve.
