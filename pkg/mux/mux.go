package mux

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
)

type errorResponse struct {
	Message string `json:"message"`
}

type Mux struct {
	patterns map[string]handleFunc
}

type handleFunc func(http.ResponseWriter, *http.Request) error

func (m *Mux) Get(pattern string, fn handleFunc) {
	k := fmt.Sprintf("%s %s", http.MethodGet, pattern)
	m.patterns[k] = fn
}

func (m *Mux) Put(pattern string, fn handleFunc) {
	k := fmt.Sprintf("%s %s", http.MethodPut, pattern)
	m.patterns[k] = fn
}

func (m *Mux) Post(pattern string, fn handleFunc) {
	k := fmt.Sprintf("%s %s", http.MethodPost, pattern)
	m.patterns[k] = fn
}

func (m *Mux) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	var err error

	defer func() {
		if err == nil {
			return
		}

		w.WriteHeader(http.StatusBadRequest)

		data, _ := json.Marshal(errorResponse{Message: err.Error()})
		io.Copy(w, bytes.NewBuffer(data))
	}()

	k := fmt.Sprintf("%s %s", r.Method, r.URL.Path)
	fn, ok := m.patterns[k]

	if !ok {
		w.WriteHeader(http.StatusNotFound)

		data, _ := json.Marshal(errorResponse{Message: "404 Not found"})
		io.Copy(w, bytes.NewBuffer(data))
	} else {
		err = fn(w, r)
	}
}

func New() *Mux {
	m := &Mux{
		patterns: make(map[string]handleFunc),
	}

	return m
}
