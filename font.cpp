#include <stdlib.h>
#include <stdio.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include "loadJsonConfig.h"
#include "font.h"

font myfont;

void initFont() {
	FILE* fontfp = fopen("assets/font.json", "r");
	if(fontfp == NULL){
		printf("Could not open font file\n");
		return;
	}
	
	printf("Loading font\n");
	jsonValue* fontjson = jsonLoad(fontfp);
	myfont.invaspect = jsonGetDouble(jsonGetObj(*fontjson, "invaspect"));
	myfont.spacing = jsonGetDouble(jsonGetObj(*fontjson, "spacing"));
	jsonValue pointsjson = jsonGetObj(*fontjson, "points");
	jsonValue formjson = jsonGetObj(*fontjson, "form");
	jsonValue letterstartjson = jsonGetObj(*fontjson, "letter");
	int pointsLen = jsonGetLen(pointsjson);
	int formLen = jsonGetLen(formjson);
	float* points = (float*)calloc(pointsLen, sizeof(float));
	short* form = (short*)calloc(formLen, sizeof(short));

	for(int idx = 0; idx < pointsLen; idx++){
		points[idx] = (float)jsonGetDouble(jsonGetArr(pointsjson, idx));
	}
	for(int idx = 0; idx < formLen; idx++){
		form[idx] = jsonGetInt(jsonGetArr(formjson, idx));
	}
	for(int idx = 0; idx < 94; idx++){
		myfont.letterStart[idx] = jsonGetInt(jsonGetArr(letterstartjson, idx));
		if(idx != 0){
			myfont.letterLen[idx-1] = (myfont.letterStart[idx]-myfont.letterStart[idx-1]);
		}
	}
	myfont.letterLen[93] = (formLen-myfont.letterStart[93]);
	
	glGenBuffers(1, &(myfont.vertex_buffer));
	glBindBuffer(GL_ARRAY_BUFFER, myfont.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*pointsLen, points, GL_STATIC_DRAW);

	glGenBuffers(1, &(myfont.ref_buffer));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, myfont.ref_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(short)*formLen, form, GL_STATIC_READ);
	
	free(form);
	free(points);
	fclose(fontfp);
	jsonFree(*fontjson);
	free(fontjson);
}
