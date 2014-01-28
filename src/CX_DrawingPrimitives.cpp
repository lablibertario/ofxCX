#include "CX_DrawingPrimitives.h"

using namespace CX::Draw;

/*!
This function draws an approximation of a squircle (http://en.wikipedia.org/wiki/Squircle) using Bezier curves.
The squircle will be centered on 0,0 in the ofPath.
\param radius The radius of the largest circle that can be enclosed in the squircle.
\param amount The squicliness of the squircle. The default (0.9) seems like a pretty good amount for a good 
approximation of a squircle, but different amounts can give different sorts of shapes.
\return An ofPath containing the squircle.
*/
ofPath CX::Draw::squircleToPath(double radius, double amount) {
	ofPath sq;

	ofPoint start, p1, p2, end;

	int s1[] = { 1, 1, -1, -1 };
	int s2[] = { 1, -1, -1, 1 };

	for (int i = 0; i < 4; i++) {
		start = ofPoint(s1[i] * radius, 0);
		p1 = ofPoint(s1[i] * radius, s2[i] * amount * radius);
		p2 = ofPoint(s1[i] * amount * radius, s2[i] * radius);
		end = ofPoint(0, s2[i] * radius);

		sq.moveTo(start);
		sq.bezierTo(p1, p2, end);
		sq.setFilled(false);
	}

	return sq;
}

/*
void squircle(ofPoint center, double radius, double rotationDeg, double amount) {
	ofPath sq = CX::Draw::squircleToPath(radius, amount);
	sq.rotate(-rotationDeg, ofVec3f(0, 0, 1));
	sq.draw(center.x, center.y);
}
*/

/*!
This draws an N-pointed star to an ofPath. The star will be centered on 0,0 in the ofPath.
\param numberOfPoints The number of points in the star.
\param innerRadius The distance from the center of the star to where the inner points of
the star hit.
\param outerRadius The distance from the center of the star to the outer points of the star.
\return An ofPath containing the star.
*/
ofPath CX::Draw::starToPath(int numberOfPoints, double innerRadius, double outerRadius) {
	ofPath star;

	bool inside = true;

	double rotationRad = -PI / 2;

	for (int i = 0; i < (2 * numberOfPoints) + 1; i++) {

		float xUnit = cos(rotationRad + (i * PI / numberOfPoints));
		float yUnit = sin(rotationRad + (i * PI / numberOfPoints));

		if (inside) {
			star.lineTo(xUnit * outerRadius, yUnit * outerRadius);
		} else {
			star.lineTo(xUnit * innerRadius, yUnit * innerRadius);
		}

		inside = !inside;

	}

	return star;
}

/*!
This draws an N-pointed star.
\param center The point at the center of the star.
\param numberOfPoints The number of points in the star.
\param innerRadius The distance from the center of the star to where the inner points of
the star hit.
\param outerRadius The distance from the center of the star to the outer points of the star.
\param lineColor The color of the lines going around the edge of the star.
\param fillColor The color used to fill in the center of the star.
\param lineWidth The width of the lines.
\param rotationDeg The number of degrees to rotate the star. 0 degrees has one point of the star pointing up. 
Positive values rotate the star counter-clockwise.
\return An ofPath containing the star.
*/
void CX::Draw::star(ofPoint center, int numberOfPoints, float innerRadius, float outerRadius,
					ofColor lineColor, ofColor fillColor, float lineWidth, float rotationDeg)
{
	ofPath star = CX::Draw::starToPath(numberOfPoints, innerRadius, outerRadius);
	star.setColor(lineColor);
	star.setFillColor(fillColor);
	star.setStrokeWidth(lineWidth);
	
	star.rotate(-rotationDeg, ofVec3f(0, 0, 1));

	star.draw(center.x, center.y);
}

void CX::Draw::centeredString(int x, int y, string s, ofTrueTypeFont &font) {
	ofRectangle bb = font.getStringBoundingBox(s, 0, 0);
	x -= bb.width / 2;
	y -= (bb.y + bb.height / 2);
	font.drawString(s, x, y);
}

void CX::Draw::centeredString(ofPoint center, string s, ofTrueTypeFont &font) {
	Draw::centeredString(center.x, center.y, s, font);
}

ofPixels CX::Draw::gaborToPixels (const CX_GaborProperties_t& properties) {
	double theta = -properties.angle * PI / 180;

	double radius = properties.width / 2;

	double slope = tan(theta);

	ofPixels pix;
	if (properties.apertureType == CX_GaborProperties_t::AP_CIRCLE) {
		pix.allocate(properties.width, properties.width, ofImageType::OF_IMAGE_COLOR_ALPHA);
	} else {
		pix.allocate(properties.width, properties.height, ofImageType::OF_IMAGE_COLOR_ALPHA);
	}
	pix.set(3, 0); //Set the alpha channel to 0 (transparent)

	//i indexes y values, j indexes x values
	for (int i = 0; i < pix.getHeight(); i++) {
		for (int j = 0; j < pix.getWidth(); j++) {

			ofPoint p(j - (pix.getWidth() / 2), i - (pix.getHeight() / 2)); //Center so that x and y are relative to the origin.

			if (properties.apertureType == CX_GaborProperties_t::AP_CIRCLE) {
				if (p.distance(ofPoint(0, 0)) >(properties.width / 2)) { //Determine radius from the width
					continue;
				}
			} else if (properties.apertureType == CX_GaborProperties_t::AP_RECTANGLE) {
				//Do nothing, allow pattern to be drawn in entire texture.
			}

			double xa = p.y / slope; //It should be ((p.y - b) / slope), but b is 0 because we go through the origin

			double hyp = abs(xa - p.x);
			double distFromA = hyp * sin(theta);

			if (slope == 0) { //Special case for flat lines.
				distFromA = p.y;
			}

			double intensity = 0;

			switch (properties.maskType) {
			case CX_GaborProperties_t::COSINE_WAVE:
				intensity = (1.0 + cos((distFromA / properties.period) * 2 * PI)) / 2.0; //Scale to be between 0 and 1
				break;
			case CX_GaborProperties_t::SQUARE_WAVE:
				if (cos((distFromA / properties.period) * 2 * PI) > 0) {
					intensity = 1;
				} else {
					intensity = 0;
				}

				break;
			case CX_GaborProperties_t::TRIANGLE_WAVE:
				double modulo = abs(fmod(distFromA, properties.period));

				if (modulo >= properties.period / 2) {
					intensity = modulo / properties.period;
				} else {
					intensity = 1 - modulo / properties.period;
				}

				break;
			}

			intensity = ofClamp(intensity, 0, 1);

			pix.setColor(j, i, ofColor(properties.color.r, properties.color.g, properties.color.b, properties.color.a * intensity));

		}
	}

	return pix;
}

ofTexture CX::Draw::gaborToTexture (const CX_GaborProperties_t& properties) {
	ofPixels pix = gaborToPixels(properties);
	ofTexture tex;
	tex.allocate(pix);
	tex.loadData(pix);
	return tex;
}

void CX::Draw::gabor (int x, int y, const CX_GaborProperties_t& properties) {
	ofTexture tex = gaborToTexture(properties);
	ofSetColor(255);
	tex.draw(x - tex.getWidth()/2, y - tex.getHeight()/2);
}