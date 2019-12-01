package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"os/user"
	"path/filepath"
	"syscall"
	"unsafe"
)

type Command struct {
	Type         int16
	SoundIndex   int16
	WaitDuration float64
	SSMLLen      int32
	SSMLPtr      uintptr
}

var (
	dll = syscall.NewLazyDLL("AudioNode.dll")

	procStart              = dll.NewProc("Start")
	procQuit               = dll.NewProc("Quit")
	procFadeIn             = dll.NewProc("FadeIn")
	procFadeOut            = dll.NewProc("FadeOut")
	procFeed               = dll.NewProc("Feed")
	procGetVoiceCount      = dll.NewProc("GetVoiceCount")
	procGetVoiceName       = dll.NewProc("GetVoiceName")
	procGetVoiceNameLength = dll.NewProc("GetVoiceNameLength")
)

func fadeInHandler(w http.ResponseWriter, r *http.Request) {
	code := int32(0)

	procFadeIn.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		log.Printf("Failed to call FadeIn() code=%d", code)
	}
}

func fadeOutHandler(w http.ResponseWriter, r *http.Request) {
	code := int32(0)

	procFadeOut.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		log.Printf("Failed to call FadeOut() code=%d", code)
	}
}

func feedHandler(w http.ResponseWriter, r *http.Request) {
	buf := &bytes.Buffer{}
	if _, err := io.Copy(buf, r.Body); err != nil {
		fmt.Printf("error while reading request body (%v)\n", err)
	}
	tts := buf.String()
	if tts == "" {
		tts = "placeholder"
	}
	ssml := fmt.Sprintf("<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='en-US'>%s</speak>", tts)

	text, err := syscall.UTF16FromString(ssml)
	if err != nil {
		log.Println(err)
	}

	textPtr, err := syscall.UTF16PtrFromString(ssml)
	if err != nil {
		log.Println(err)
	}

	textLength := int32(len(text))

	code := int32(0)

	cmds := make([]uintptr, 2, 2)

	cmd1 := Command{
		Type:       1,
		SoundIndex: 0,
	}
	cmds[0] = uintptr(unsafe.Pointer(&cmd1))

	cmd2 := Command{
		Type:    3,
		SSMLPtr: uintptr(unsafe.Pointer(textPtr)),
		SSMLLen: textLength,
	}
	cmds[1] = uintptr(unsafe.Pointer(&cmd2))
	procFeed.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&cmds[0])), uintptr(2))

	if code != 0 {
		log.Printf("Failed to call Feed() code=%d", code)
	}
}

func voicesHandler(w http.ResponseWriter, r *http.Request) {
	code := int32(0)
	numberOfVoices := int32(0)

	procGetVoiceCount.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(&numberOfVoices)))

	if code != 0 {
		log.Printf("Failed to call GetVoiceCount() code=%d", code)
	}

	voiceNames := []string{}
	voiceNameLength := int32(0)

	for i := int32(0); i < numberOfVoices; i++ {
		procGetVoiceNameLength.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&voiceNameLength)))

		if code != 0 {
			log.Printf("Failed to call GetVoiceNameLength() code=%d", code)
			continue
		}

		voiceName := make([]uint16, voiceNameLength)

		procGetVoiceName.Call(uintptr(unsafe.Pointer(&code)), uintptr(i), uintptr(unsafe.Pointer(&voiceName)))

		if code != 0 {
			log.Printf("Failed to call GetVoiceName() code=%d", code)
			continue
		}

		voiceNames = append(voiceNames, syscall.UTF16ToString(voiceName))
	}
	data, err := json.Marshal(struct {
		Voices []string `json:"voices"`
	}{
		Voices: voiceNames,
	})
	if err != nil {
		log.Fatal("failed to call json.Marshal()")
		return
	}

	io.Copy(w, bytes.NewBuffer(data))
}

func startHandler(w http.ResponseWriter, r *http.Request) {
	u, err := user.Current()

	if err != nil {
		fmt.Println(err)
	}

	fullLogPath := filepath.Join(u.HomeDir, "AppData", "Roaming", "YetAnotherNarrater", "Log", "AudioAgent.ltsv")

	os.MkdirAll(filepath.Dir(fullLogPath), 0755)

	fmt.Println(fullLogPath)
	u16ptr, err := syscall.UTF16PtrFromString(fullLogPath)

	if err != nil {
		fmt.Println(err)
		return
	}
	code := int32(0)

	procStart.Call(uintptr(unsafe.Pointer(&code)), uintptr(unsafe.Pointer(u16ptr)), uintptr(0))

	if code != 0 {
		log.Printf("Failed to call Start()")
	}
}

func quitHandler(w http.ResponseWriter, r *http.Request) {
	code := int32(0)

	procQuit.Call(uintptr(unsafe.Pointer(&code)))

	if code != 0 {
		log.Printf("Failed to call Quit()")
	}
}

func main() {
	mux := http.NewServeMux()

	mux.HandleFunc("/v1/engine/fadein", fadeInHandler)
	mux.HandleFunc("/v1/engine/fadeout", fadeOutHandler)
	mux.HandleFunc("/v1/engine/feed", feedHandler)
	mux.HandleFunc("/v1/engine/voices", voicesHandler)
	mux.HandleFunc("/v1/engine/start", startHandler)
	mux.HandleFunc("/v1/engine/quit", quitHandler)

	server := &http.Server{
		Addr:    ":4000",
		Handler: mux,
	}

	if err := server.ListenAndServe(); err != nil {
		panic(err)
	}

	defer server.Close()
}
