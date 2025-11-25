import torch
from transformers import (
    # --- TrOCR classes are removed ---
    # NEW imports for Model 1
    Pix2StructForConditionalGeneration,
    Pix2StructProcessor,
    # --- Model 2 and 3 classes are the same ---
    T5ForConditionalGeneration,
    T5Tokenizer,
    pipeline,
)
from PIL import Image
import requests

img_path = "positive.png" 

# --- Setup device ---
if torch.cuda.is_available():
    device = torch.device("cuda")
    print(f"CUDA is available! Using GPU: {torch.cuda.get_device_name(0)}")
else:
    device = torch.device("cpu")
    print("CUDA not available. Using CPU.")

# --- Models ---
print("Preparing models")

# --- Model 1: OCR (REPLACED) ---
# Use Pix2Struct, which is trained on screenshots
model_1_name = "google/pix2struct-base"
ocr_processor = Pix2StructProcessor.from_pretrained(model_1_name)
ocr_model = Pix2StructForConditionalGeneration.from_pretrained(model_1_name)
ocr_model.to(device) #enable hardware acceleration

# --- Model 2: Summarizer (Unchanged) ---
summarizer_tokenizer = T5Tokenizer.from_pretrained("t5-small")
summarizer_model = T5ForConditionalGeneration.from_pretrained("t5-small")
summarizer_model.to(device)

# --- Model 3: Sentiment (Unchanged) ---
sentiment_classifier = pipeline(
    "sentiment-analysis",
    model="distilbert-base-uncased-finetuned-sst-2-english",
    device=0 if device.type == "cuda" else -1
)

print("DONE preparing models")

image = Image.open(img_path).convert("RGB")

# --- Step 1: Running OCR Model (Updated) ---
print("\n--- Step 1: Running OCR Model (Pix2Struct) ---")

# Pix2Struct's processor handles the image differently
# We don't need a text prompt, just the image
inputs = ocr_processor(images=image, return_tensors="pt")
# Move all parts of the input to the GPU
inputs = {key: val.to(device) for key, val in inputs.items()}

with torch.no_grad():
    # Use max_new_tokens to ensure it can generate the full text
    generated_ids = ocr_model.generate(**inputs, max_new_tokens=512)
    
out1_raw_text = ocr_processor.batch_decode(generated_ids, skip_special_tokens=True)[0]
print(f"Extracted Text: \n{out1_raw_text}")


# --- Step 2: Running Summarization Model (Unchanged) ---
print("\n--- Step 2: Running Summarization Model ---")

# Now, 'out1_raw_text' should contain the real review text!
text_to_summarize = "summarize: " + out1_raw_text
inputs = summarizer_tokenizer(text_to_summarize, return_tensors="pt", max_length=512, truncation=True)
input_ids = inputs.input_ids.to(device)

with torch.no_grad():
    summary_ids = summarizer_model.generate(
        input_ids,
        num_beams=4,
        max_length=50, # You can make this longer if you want
        early_stopping=True
    )

out2_summary = summarizer_tokenizer.decode(summary_ids[0].cpu(), skip_special_tokens=True)
print(f"Summarized Text: \n{out2_summary}")

# --- Step 3: Running Sentiment Analysis Model (Unchanged) ---
print("\n--- Step 3: Running Sentiment Analysis Model ---")

# Model 3 will now get the *real summary* as input
out3_sentiment = sentiment_classifier(out2_summary)

print(f"\n--- FINAL PIPELINE OUTPUT ---")
print(f"Sentiment Analysis Result: {out3_sentiment[0]}")
