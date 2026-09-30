#pragma once
#include <cmath>

class Complex
{
private:
	double real;
	double imaginary;
public:
	Complex(double real = 0, double imaginary = 0);
	Complex(const Complex& other);

	double getReal() const { return real; }
	double getImaginary() const { return imaginary; }
	void setReal(double r) { real = r; }
	void setImaginary(double img) { imaginary = img; }
	double phase() const { return atan2(imaginary, real); }

	double magnitude() const;
	Complex conjugate() const;

	Complex operator+(const Complex& other) const;
	Complex operator-(const Complex& other) const;
	Complex operator*(const Complex& other) const;
	Complex operator/(double scale) const;
};
