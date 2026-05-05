#!/usr/bin/env python3
"""
Example script showing how to train a simple custom classifier
and export it to TorchScript format for the camera classifier app.
"""

import torch
import torch.nn as nn
import torch.optim as optim
from torchvision import models, transforms, datasets
from torch.utils.data import DataLoader
import argparse
import os


def create_model(num_classes=5, architecture='resnet18'):
    """Create a model with custom number of classes."""
    if architecture == 'resnet18':
        model = models.resnet18(pretrained=True)
        num_features = model.fc.in_features
        model.fc = nn.Linear(num_features, num_classes)
    elif architecture == 'mobilenet_v2':
        model = models.mobilenet_v2(pretrained=True)
        num_features = model.classifier[1].in_features
        model.classifier[1] = nn.Linear(num_features, num_classes)
    else:
        raise ValueError(f"Unknown architecture: {architecture}")
    
    return model


def train_model(model, train_loader, val_loader, num_epochs=10, device='cuda'):
    """Simple training loop."""
    model = model.to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=0.001)
    scheduler = optim.lr_scheduler.StepLR(optimizer, step_size=5, gamma=0.1)
    
    best_acc = 0.0
    
    for epoch in range(num_epochs):
        print(f"\nEpoch {epoch+1}/{num_epochs}")
        print("-" * 40)
        
        # Training phase
        model.train()
        running_loss = 0.0
        running_corrects = 0
        
        for inputs, labels in train_loader:
            inputs = inputs.to(device)
            labels = labels.to(device)
            
            optimizer.zero_grad()
            
            outputs = model(inputs)
            _, preds = torch.max(outputs, 1)
            loss = criterion(outputs, labels)
            
            loss.backward()
            optimizer.step()
            
            running_loss += loss.item() * inputs.size(0)
            running_corrects += torch.sum(preds == labels.data)
        
        scheduler.step()
        
        epoch_loss = running_loss / len(train_loader.dataset)
        epoch_acc = running_corrects.double() / len(train_loader.dataset)
        
        print(f"Train Loss: {epoch_loss:.4f} Acc: {epoch_acc:.4f}")
        
        # Validation phase
        model.eval()
        val_corrects = 0
        
        with torch.no_grad():
            for inputs, labels in val_loader:
                inputs = inputs.to(device)
                labels = labels.to(device)
                
                outputs = model(inputs)
                _, preds = torch.max(outputs, 1)
                val_corrects += torch.sum(preds == labels.data)
        
        val_acc = val_corrects.double() / len(val_loader.dataset)
        print(f"Val Acc: {val_acc:.4f}")
        
        if val_acc > best_acc:
            best_acc = val_acc
    
    print(f"\nBest validation accuracy: {best_acc:.4f}")
    return model


def export_to_torchscript(model, output_path, num_classes):
    """Export trained model to TorchScript."""
    model.eval()
    
    # Create example input
    example_input = torch.rand(1, 3, 224, 224)
    
    # Trace the model
    traced_model = torch.jit.trace(model, example_input)
    
    # Optimize for inference
    traced_model = torch.jit.optimize_for_inference(traced_model)
    
    # Save
    traced_model.save(output_path)
    
    print(f"\nModel exported to TorchScript: {output_path}")
    print(f"Number of classes: {num_classes}")
    print("\nTo use with camera classifier:")
    print(f"  ./camera_classifier --camera 0 --model {output_path}")


def main():
    parser = argparse.ArgumentParser(description='Train and export a custom classifier')
    parser.add_argument('--data-dir', type=str, required=True,
                       help='Directory containing train/ and val/ subdirectories with class folders')
    parser.add_argument('--output', type=str, default='custom_model.pt',
                       help='Output path for TorchScript model')
    parser.add_argument('--architecture', type=str, default='resnet18',
                       choices=['resnet18', 'mobilenet_v2'],
                       help='Model architecture')
    parser.add_argument('--epochs', type=int, default=10,
                       help='Number of training epochs')
    parser.add_argument('--batch-size', type=int, default=32,
                       help='Batch size')
    parser.add_argument('--no-train', action='store_true',
                       help='Skip training, just export a random initialized model')
    
    args = parser.parse_args()
    
    # Check CUDA availability
    device = 'cuda' if torch.cuda.is_available() else 'cpu'
    print(f"Using device: {device}")
    
    # Data transforms
    data_transforms = {
        'train': transforms.Compose([
            transforms.RandomResizedCrop(224),
            transforms.RandomHorizontalFlip(),
            transforms.ToTensor(),
            transforms.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225])
        ]),
        'val': transforms.Compose([
            transforms.Resize(256),
            transforms.CenterCrop(224),
            transforms.ToTensor(),
            transforms.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225])
        ])
    }
    
    if not args.no_train:
        # Load datasets
        print(f"Loading data from {args.data_dir}...")
        train_dataset = datasets.ImageFolder(
            os.path.join(args.data_dir, 'train'),
            data_transforms['train']
        )
        val_dataset = datasets.ImageFolder(
            os.path.join(args.data_dir, 'val'),
            data_transforms['val']
        )
        
        train_loader = DataLoader(train_dataset, batch_size=args.batch_size,
                                 shuffle=True, num_workers=4)
        val_loader = DataLoader(val_dataset, batch_size=args.batch_size,
                               shuffle=False, num_workers=4)
        
        num_classes = len(train_dataset.classes)
        class_names = train_dataset.classes
        
        print(f"Number of classes: {num_classes}")
        print(f"Class names: {class_names}")
        print(f"Training samples: {len(train_dataset)}")
        print(f"Validation samples: {len(val_dataset)}")
        
        # Create and train model
        model = create_model(num_classes, args.architecture)
        model = train_model(model, train_loader, val_loader, args.epochs, device)
        
        # Save class names
        with open(args.output.replace('.pt', '_classes.txt'), 'w') as f:
            for class_name in class_names:
                f.write(f"{class_name}\n")
        
    else:
        # Just create a model for export (for testing)
        num_classes = 5
        class_names = [f"class_{i}" for i in range(num_classes)]
        model = create_model(num_classes, args.architecture)
        print(f"Created model with {num_classes} random classes")
    
    # Export to TorchScript
    export_to_torchscript(model, args.output, num_classes)
    
    print("\nTo update class names in the C++ app, edit src/main.cpp:")
    print("classifier_thread.loadClassNames({")
    for class_name in class_names:
        print(f'    "{class_name}",')
    print("});")


if __name__ == "__main__":
    main()
