// color.rs -- color handling
// Copyright (C) 2015 Alex Iadicicco

//! Color management

/// A simple struct representing an RGB color triplet
#[derive(Clone, Copy)]
pub struct Color {
    pub r: f32,
    pub g: f32,
    pub b: f32,
}

fn clamp(x: f32, a: f32, b: f32) -> f32 {
    if       x < a { a }
    else if  x > b { b }
    else           { x }
}

impl Color {
    /// Creates a new color with the given components
    pub fn new(r: f32, g: f32, b: f32) -> Color {
        Color { r: r, g: g, b: b }
    }

    /// Creates a new color with the given components as 64 bit floats
    pub fn new_f32(r: f32, g: f32, b: f32) -> Color {
        Color::new(r as f32, g as f32, b as f32)
    }

    /// Creates a new color by mixing this and another color according to the
    /// given mix factor. When f = 0.0, the output will be entirely the current
    /// color. When f = 1.0, the output will be entirely the other color.
    pub fn mix(&self, other: &Color, f: f32) -> Color {
        let f = clamp(f, 0.0, 1.0);
        Color {
            r: self.r * (1.0 - f) + other.r * f,
            g: self.g * (1.0 - f) + other.g * f,
            b: self.b * (1.0 - f) + other.b * f,
        }
    }

    /// Returns an 8 bit integer for the red component
    pub fn r8(&self) -> u8 { clamp(self.r * 255.0, 0.0, 255.0) as u8 }

    /// Returns an 8 bit integer for the green component
    pub fn g8(&self) -> u8 { clamp(self.g * 255.0, 0.0, 255.0) as u8 }

    /// Returns an 8 bit integer for the blue component
    pub fn b8(&self) -> u8 { clamp(self.b * 255.0, 0.0, 255.0) as u8 }
}
