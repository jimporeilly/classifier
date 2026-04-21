#!/usr/bin/env python3
"""
Script to export a PyTorch model to TorchScript format for use with the classifier app.
"""

import torch
import torchvision.models as models
import argparse


def export_model(model_name='resnet18', output_path='model.pt', pretrained=True):
    """
    Export a torchvision model to TorchScript format.
    
    Args:
        model_name: Name of the model from torchvision.models
        output_path: Path where to save the TorchScript model
        pretrained: Whether to use pretrained weights
    """
    print(f"Loading {model_name}...")
    
    # Load model
    if model_name == 'resnet18':
        model = models.resnet18(pretrained=pretrained)
    elif model_name == 'resnet50':
        model = models.resnet50(pretrained=pretrained)
    elif model_name == 'mobilenet_v2':
        model = models.mobilenet_v2(pretrained=pretrained)
    elif model_name == 'efficientnet_b0':
        model = models.efficientnet_b0(pretrained=pretrained)
    elif model_name == 'squeezenet1_1':
        model = models.squeezenet1_1(pretrained=pretrained)
    else:
        raise ValueError(f"Unknown model: {model_name}")
    
    # Set to evaluation mode
    model.eval()
    
    # Create example input (batch_size=1, channels=3, height=224, width=224)
    example_input = torch.rand(1, 3, 224, 224)
    
    print("Tracing model...")
    # Trace the model
    traced_model = torch.jit.trace(model, example_input)
    
    # Optimize for inference
    traced_model = torch.jit.optimize_for_inference(traced_model)
    
    print(f"Saving to {output_path}...")
    # Save the traced model
    traced_model.save(output_path)
    
    # Verify the model can be loaded
    print("Verifying saved model...")
    loaded_model = torch.jit.load(output_path)
    
    # Test inference
    with torch.no_grad():
        output = loaded_model(example_input)
    
    print(f"Model exported successfully!")
    print(f"  Input shape: {example_input.shape}")
    print(f"  Output shape: {output.shape}")
    print(f"  Number of classes: {output.shape[1]}")
    print(f"  Model file: {output_path}")


def export_custom_classifier(num_classes=5, output_path='custom_model.pt'):
    """
    Export a simple custom classifier for demonstration.
    
    Args:
        num_classes: Number of output classes
        output_path: Path where to save the model
    """
    print(f"Creating custom classifier with {num_classes} classes...")
    
    # Create a simple classifier based on ResNet18
    model = models.resnet18(pretrained=True)
    
    # Modify the final layer for custom number of classes
    num_features = model.fc.in_features
    model.fc = torch.nn.Linear(num_features, num_classes)
    
    model.eval()
    
    # Create example input
    example_input = torch.rand(1, 3, 224, 224)
    
    print("Tracing model...")
    traced_model = torch.jit.trace(model, example_input)
    traced_model = torch.jit.optimize_for_inference(traced_model)
    
    print(f"Saving to {output_path}...")
    traced_model.save(output_path)
    
    print(f"Custom classifier exported successfully!")
    print(f"  Number of classes: {num_classes}")
    print(f"  Model file: {output_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Export PyTorch models to TorchScript')
    parser.add_argument('--model', type=str, default='resnet18',
                       choices=['resnet18', 'resnet50', 'mobilenet_v2', 
                               'efficientnet_b0', 'squeezenet1_1', 'custom'],
                       help='Model architecture to export')
    parser.add_argument('--output', type=str, default='model.pt',
                       help='Output path for the TorchScript model')
    parser.add_argument('--pretrained', action='store_true', default=True,
                       help='Use pretrained weights')
    parser.add_argument('--num-classes', type=int, default=5,
                       help='Number of classes for custom model')
    
    args = parser.parse_args()
    
    if args.model == 'custom':
        export_custom_classifier(args.num_classes, args.output)
    else:
        export_model(args.model, args.output, args.pretrained)
    
    print("\nTo use this model with the classifier app:")
    print(f"  ./camera_classifier --camera 0 --model {args.output}")
