package app

import (
	"context"
	"fmt"
	"net/http"
	"sync"

	"github.com/moutend/AudioNode/pkg/api"
	"github.com/moutend/AudioNode/pkg/mux"
)

type app struct {
	m         *sync.Mutex
	wg        *sync.WaitGroup
	server    *http.Server
	isRunning bool
}

func (a *app) setup() error {
	mux := mux.New()

	mux.Post("/v1/audio/command", api.PostAudioCommand)
	mux.Get("/v1/audio/enable", api.GetAudioEnable)
	mux.Get("/v1/audio/disable", api.GetAudioDisable)
	mux.Get("/v1/audio/restart", api.GetAudioRestart)
	mux.Get("/v1/audio/pause", api.GetAudioPause)

	mux.Get("/v1/voices", api.GetVoices)
	mux.Post("/v1/voice", api.PostVoice)
	mux.Post("/v1/voice/rate", api.PostVoiceRate)
	mux.Post("/v1/voice/pitch", api.PostVoicePitch)
	mux.Post("/v1/voice/volume", api.PostVoiceVolume)

	a.server = &http.Server{
		Addr:    ":7902",
		Handler: mux,
	}

	a.wg.Add(1)

	go func() {
		if err := a.server.ListenAndServe(); err != http.ErrServerClosed {
			panic(err)
		}
		a.wg.Done()
	}()

	return nil
}

func (a *app) Setup() error {
	a.m.Lock()
	defer a.m.Unlock()

	if a.isRunning {
		fmt.Errorf("Setup is already done")
	}
	if err := a.setup(); err != nil {
		return err
	}

	a.isRunning = true

	return nil
}

func (a *app) teardown() error {
	if err := a.server.Shutdown(context.TODO()); err != nil {
		return err
	}

	a.wg.Wait()

	return nil
}

func (a *app) Teardown() error {
	a.m.Lock()
	defer a.m.Unlock()

	if !a.isRunning {
		return fmt.Errorf("Teardown is already done")
	}
	if err := a.teardown(); err != nil {
		return err
	}

	a.isRunning = false

	return nil
}

func New() *app {
	return &app{
		m:  &sync.Mutex{},
		wg: &sync.WaitGroup{},
	}
}
