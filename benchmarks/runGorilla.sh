#!/bin/bash
GOPATH=~ go get github.com/gorilla/websocket
GOPATH=~ GOMAXPROCS=1 go run gorilla.go
