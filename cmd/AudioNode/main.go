package main

import (
	"encoding/hex"
	"fmt"
	"log"
	"math/rand"
	"os"
	"os/signal"
	"os/user"
	"path/filepath"
	"time"

	"github.com/moutend/AudioNode/pkg/app"
	"github.com/moutend/AudioNode/pkg/types"
)

func main() {
	if err := run(os.Args); err != nil {
		log.New(os.Stderr, "", 0).Fatal(err)
	}
}

func run(args []string) error {
	rand.Seed(time.Now().Unix())
	p := make([]byte, 16)

	if _, err := rand.Read(p); err != nil {
		return err
	}

	u, err := user.Current()

	if err != nil {
		return err
	}

	fileName := fmt.Sprintf("AudioNode-%s.txt", hex.EncodeToString(p))
	outputPath := filepath.Join(u.HomeDir, "AppData", "Roaming", "ScreenReaderX", "SystemLog", fileName)
	os.MkdirAll(filepath.Dir(outputPath), 0755)

	output := types.NewBackgroundWriter(outputPath)
	defer output.Close()

	log.SetFlags(log.Ldate | log.Ltime | log.LUTC | log.Llongfile)
	log.SetOutput(output)

	app := app.New()

	if err := app.Setup(); err != nil {
		return err
	}

	log.Println("Setup is completed")

	interrupt := make(chan os.Signal, 1)
	signal.Notify(interrupt, os.Interrupt)

	<-interrupt

	if err := app.Teardown(); err != nil {
		return err
	}

	log.Println("Teardown is completed")

	return nil
}
