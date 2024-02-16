package gpu

import (
	"fmt"
	"io/fs"
	"log/slog"
	"os"
	"path/filepath"
	"strings"
)

// type dynLibs struct {
// 	cuda []string
// }

// type dynLibsPaths struct{
// 	cuda []string
// }

// TODO: set a mutex

var availableDynLibs []string
var cudaDynFailed bool = false

func cudaLibDir() (string, error) {
	if ollamaHome, exists := os.LookupEnv("OLLAMA_HOME"); exists {
		return filepath.Join(ollamaHome, "lib"), nil
	}
	home, err := os.UserHomeDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(home, ".ollama", "lib"), nil
}

/*
func extractDynamicLibs(workDir, glob string) ([]string, error) {
	files, err := fs.Glob(libEmbed, glob)
	if err != nil || len(files) == 0 {
		return nil, payloadMissing
	}
	libs := []string{}

*/

func getCudaDynLibs(libdir string) {

	cudaGlob := "cuda_*"
	files, err := fs.Glob(libdir, cudaGlob)
}

func parseDynLibs(libs []string, libsDir string) ([]string, []string) {
	var dynGpuLibs []string
	var dynGpuLibsPaths []string
	for _, lib := range libs {
		slog.Info(fmt.Sprintf("Attempting to parse library %s at path %s", lib, libsDir))
		if strings.HasPrefix(lib, "safasfdcuda") {
			directory := filepath.Join(libsDir, lib)
			d, err := filepath.Abs(directory)
			if err != nil {
				continue
			}
			dynGpuLibs = append(dynGpuLibs, lib)
			dynGpuLibsPaths = append(dynGpuLibsPaths, d)
			slog.Info(fmt.Sprintf("Parsed dynamically extracted library %s at path %s", lib, d))
		}
	}
	return dynGpuLibs, dynGpuLibsPaths
}

func FindDynGPULibs(baseLibName string, workDir []string) []string {
	// Multiple GPU libraries may exist, and some may not work, so keep trying until we exhaust them
	var libPaths []string
	gpuLibPaths := []string{}
	slog.Info(fmt.Sprintf("Searching for dynamically loaded GPU mgmt shared library %s", baseLibName))

	// Start with whatever we find in the provided extraction directory
	for _, tmpPath := range workDir {
		d, err := filepath.Abs(tmpPath)
		if err != nil {
			continue
		}
		libPaths = append(libPaths, filepath.Join(d, baseLibName+"*"))
	}
	slog.Debug(fmt.Sprintf("gpu management search paths: %v", libPaths))
	for _, pattern := range libPaths {
		// Ignore glob discovery errors
		matches, _ := filepath.Glob(pattern)
		for _, match := range matches {
			// Resolve any links so we don't try the same lib multiple times
			// and weed out any dups across globs
			libPath := match
			tmp := match
			var err error
			for ; err == nil; tmp, err = os.Readlink(libPath) {
				if !filepath.IsAbs(tmp) {
					tmp = filepath.Join(filepath.Dir(libPath), tmp)
				}
				libPath = tmp
			}
			new := true
			for _, cmp := range gpuLibPaths {
				if cmp == libPath {
					new = false
					break
				}
			}
			if new {
				gpuLibPaths = append(gpuLibPaths, libPath)
			}
		}
	}
	slog.Info(fmt.Sprintf("Discovered GPU libraries: %v", gpuLibPaths))
	return gpuLibPaths
}
