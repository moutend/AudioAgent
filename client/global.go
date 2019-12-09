package main

import (
	"io/ioutil"
	"log"
	"syscall"
)

var (
	logger  = log.New(ioutil.Discard, "discard logging messages", log.LstdFlags)
	logPath = ""
	dll     = syscall.NewLazyDLL("AudioNode.dll")

	procStart                     = dll.NewProc("Start")
	procQuit                      = dll.NewProc("Quit")
	procFadeIn                    = dll.NewProc("FadeIn")
	procFadeOut                   = dll.NewProc("FadeOut")
	procFeed                      = dll.NewProc("Feed")
	procGetVoiceCount             = dll.NewProc("GetVoiceCount")
	procGetVoiceId                = dll.NewProc("GetVoiceId")
	procGetVoiceIdLength          = dll.NewProc("GetVoiceIdLength")
	procGetVoiceDisplayName       = dll.NewProc("GetVoiceDisplayName")
	procGetVoiceDisplayNameLength = dll.NewProc("GetVoiceDisplayNameLength")
	procGetVoiceLanguage          = dll.NewProc("GetVoiceLanguage")
	procGetVoiceLanguageLength    = dll.NewProc("GetVoiceLanguageLength")
	procGetDefaultVoice           = dll.NewProc("GetDefaultVoice")
	procSetDefaultVoice           = dll.NewProc("SetDefaultVoice")
	procGetSpeakingRate           = dll.NewProc("GetSpeakingRate")
	procSetSpeakingRate           = dll.NewProc("SetSpeakingRate")
)
