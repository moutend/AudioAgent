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

	procSetup                     = dll.NewProc("Setup")
	procTeardown                  = dll.NewProc("Teardown")
	procFadeIn                    = dll.NewProc("FadeIn")
	procFadeOut                   = dll.NewProc("FadeOut")
	procForcePush                 = dll.NewProc("ForcePush")
	procPush                      = dll.NewProc("Push")
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
	procGetAudioPitch             = dll.NewProc("GetAudioPitch")
	procSetAudioPitch             = dll.NewProc("SetAudioPitch")
	procGetAudioVolume            = dll.NewProc("GetAudioVolume")
	procSetAudioVolume            = dll.NewProc("SetAudioVolume")
)
