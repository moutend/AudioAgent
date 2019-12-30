package main

import (
	"log"
	"net/http"
	"os"
)

func main() {
	file, err := os.Create("log.txt")

	if err != nil {
		panic(err)
	}

	logger = log.New(file, "", log.Lshortfile|log.LUTC)

	mux := NewMux()

	mux.Post("/v1/audio/command", postAudioCommand)
	mux.Get("/v1/audio/enable", getAudioEnable)
	mux.Get("/v1/audio/disable", getAudioDisable)
	mux.Get("/v1/audio/restart", getAudioRestart)
	mux.Get("/v1/audio/pause", getAudioPause)

	mux.Get("/v1/voices", getVoices)
	mux.Post("/v1/voice", postVoice)
	mux.Post("/v1/voice/rate", postVoiceRate)
	mux.Post("/v1/voice/pitch", postVoicePitch)
	mux.Post("/v1/voice/volume", postVoiceVolume)

	server := &http.Server{
		Addr:    ":4000",
		Handler: mux,
	}

	if err := server.ListenAndServe(); err != nil {
		panic(err)
	}
	defer server.Close()
}
