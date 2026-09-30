#include "Complex.h"
#include <cmath>

Complex::Complex(double r, double i) {
	real = r;
	imaginary = i;
}
Complex::Complex(const Complex& other) {
	real = other.real;
	imaginary = other.imaginary;
}

double Complex::magnitude() const {
	return sqrt(real * real + imaginary * imaginary);
}
Complex Complex::conjugate() const {
	return Complex(real, -imaginary);
}

//operation on complex numbers basic
Complex Complex::operator+(const Complex& other) const {
	double realSum = real + other.real;
	double imaginarySum = imaginary + other.imaginary;
	return Complex(realSum, imaginarySum);
}
Complex Complex::operator-(const Complex& other) const {
	double realSum = real - other.real;
	double imaginarySum = imaginary - other.imaginary;
	return Complex(realSum, imaginarySum);
}
Complex Complex::operator*(const Complex& other) const {
	double realTotal = real * other.real - imaginary * other.imaginary;
	double imaginaryTotal = real * other.imaginary + imaginary * other.real;

	return Complex(realTotal, imaginaryTotal);
}
Complex Complex::operator/(double scale) const {
	double realTotal = real / scale;
	double imaginaryTotal = imaginary / scale;
	return Complex(realTotal, imaginaryTotal);

}