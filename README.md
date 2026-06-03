# izi

izi is a simple, fast, AI-ready programming language.

## Vision

izi aims to combine:

- Go-like simplicity
- Python-like productivity
- C++/Rust-like performance
- Native AI tools

## Primary Use Cases

The language is being designed for:

- backend servers
- CLI tools
- scripting
- data processing
- machine learning and AI
- high-performance modules
- games or systems programming later

## Compiler Architecture

The core compilation pipeline is:

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

Planned CLI workflow:

```bash
izi run main.izi
izi build main.izi
```

## Syntax Direction

```izi
import ai.tensor

fn main() {
    let x: Tensor = tensor([[1, 2], [3, 4]])
    let y = x * 2

    print(y)
}
```

Functions:

```izi
fn add(a: int, b: int) -> int {
    return a + b
}
```

Structs and methods:

```izi
struct User {
    name: string
    age: int
}

fn User.sayHello(self) {
    print("Hello " + self.name)
}
```

## AI-First Standard Modules

Planned built-in modules include:

- `izi.tensor`
- `izi.nn`
- `izi.optim`
- `izi.data`
- `izi.gpu`
- `izi.math`
- `izi.image`
- `izi.audio`

Example long-term AI usage:

```izi
import ai.nn
import ai.optim

model := nn.Sequential([
    nn.Linear(784, 128),
    nn.ReLU(),
    nn.Linear(128, 10)
])

optimizer := optim.Adam(model.params(), lr: 0.001)
```

## Roadmap

### Phase 1 — Core language

- variables
- types
- functions
- if/else
- loops
- arrays
- structs
- modules
- compiler using LLVM

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

## Early Implementation Strategy

The first version should focus on native compilation through LLVM and lean on
interoperability for AI workloads instead of rebuilding a full AI ecosystem too
early.

For example:

```izi
import py.torch as torch
```

This keeps the initial compiler scope practical while still supporting the
language's AI-first direction.