package main

import (
	"fmt"
	"log"
	"net/http"
	"os"
	"os/user"
	"path/filepath"
)

func main() {
	file, err := os.Create("log.txt")

	if err != nil {
		panic(err)
	}

	logger = log.New(file, "", log.Lshortfile|log.LUTC)

	u, err := user.Current()

	if err != nil {
		log.Fatal(err)
	}

	logPath = filepath.Join(u.HomeDir, "AppData", "Roaming", "YetAnotherNarrater", "Log")
	os.MkdirAll(logPath, 0755)

	fmt.Println(logPath)

	mux := http.NewServeMux()

	mux.HandleFunc("/v1/audio", audioHandler)
	mux.HandleFunc("/v1/voices", voicesHandler)
	// mux.HandleFunc("/v1/sfx", sfxHandler)

	server := &http.Server{
		Addr:    ":4000",
		Handler: mux,
	}

	if err := server.ListenAndServe(); err != nil {
		panic(err)
	}
	defer server.Close()
}
