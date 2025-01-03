import wave
import numpy as np
import struct
import os
from pathlib import Path

class AudioConverter:
    def __init__(self, target_duration=1.0, sample_rate=44100):
        self.target_duration = target_duration
        self.sample_rate = sample_rate
        self.target_samples = int(target_duration * sample_rate)
    
    def convert_wav_to_pcm(self, input_file):
        with wave.open(input_file, 'rb') as wav_file:
            # Read wave file parameters
            n_channels = wav_file.getnchannels()
            sampwidth = wav_file.getsampwidth()
            framerate = wav_file.getframerate()
            n_frames = wav_file.getnframes()
            
            # Read all frames
            frames = wav_file.readframes(n_frames)
            
            # Convert to numpy array
            if sampwidth == 2:
                data = np.frombuffer(frames, dtype=np.int16)
            else:
                raise ValueError("Only 16-bit WAV files are supported")
            
            # Convert stereo to mono if necessary
            if n_channels == 2:
                data = data[::2]  # Take only left channel
            
            # Resample if necessary
            if framerate != self.sample_rate:
                # Simple resampling (you might want to use a proper resampling library)
                old_indices = np.arange(len(data))
                new_indices = np.linspace(0, len(data)-1, int(len(data) * self.sample_rate / framerate))
                data = np.interp(new_indices, old_indices, data)
            
            # Pad or truncate to target duration
            final_data = np.zeros(self.target_samples, dtype=np.int16)
            if len(data) > self.target_samples:
                final_data = data[:self.target_samples]
            else:
                final_data[:len(data)] = data
            
            return final_data

    def create_spiffs_file(self, input_directory, output_file):
        wav_files = sorted(Path(input_directory).glob('*.wav'))
        
        if not wav_files:
            raise ValueError(f"No WAV files found in {input_directory}")
        
        # Convert and concatenate all files
        all_data = []
        for wav_file in wav_files:
            print(f"Converting {wav_file.name}...")
            pcm_data = self.convert_wav_to_pcm(str(wav_file))
            all_data.append(pcm_data)
        
        # Concatenate all audio data
        combined_data = np.concatenate(all_data)
        
        # Save to binary file
        with open(output_file, 'wb') as f:
            combined_data.tofile(f)
        
        print(f"\nCreated combined PCM file: {output_file}")
        print(f"Total sounds: {len(wav_files)}")
        print(f"Each sound block size: {self.target_samples * 2} bytes")
        print(f"Total file size: {os.path.getsize(output_file)} bytes")
        
        return len(wav_files)

if __name__ == "__main__":
    # Create converter with 1-second blocks at 44.1kHz
    converter = AudioConverter(target_duration=1.0, sample_rate=44100)
    
    # Convert files
    input_dir = "audio_files"  # Put your WAV files in this directory
    output_file = "sounds.pcm"
    
    try:
        num_sounds = converter.create_spiffs_file(input_dir, output_file)
        print("\nConversion successful!")
        print("\nTo use in ESP-IDF:")
        print(f"1. Copy {output_file} to your project's spiffs_image directory")
        print("2. Update your partitions to ensure enough SPIFFS space")
        print(f"3. Update your code with NUM_SOUNDS = {num_sounds}")
    except Exception as e:
        print(f"Error: {e}")