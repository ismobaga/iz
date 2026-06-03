GO_FILES := $(shell find . -name '*.go' -not -path './vendor/*')

.PHONY: fmt fmt-check test build

fmt:
gofmt -w $(GO_FILES)

fmt-check:
@test -z "$(shell gofmt -l $(GO_FILES))"

test:
go test ./...

build:
go build ./...
