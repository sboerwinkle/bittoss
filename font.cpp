#include <stdlib.h>
#include <stdio.h>
#include <GL/gl.h>
#include <GLES3/gl3.h>
#include "json.h"
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

	myfont.invaspect = fontjson->get("invaspect")->getDouble();
	myfont.spacing = fontjson->get("spacing")->getDouble();
	list<jsonValue> *pointsjson = fontjson->get("points")->getItems();
	list<jsonValue> *formjson = fontjson->get("form")->getItems();
	list<jsonValue> *letterstartjson = fontjson->get("letter")->getItems();
	int pointsLen = pointsjson->num;
	int formLen = formjson->num;
	int numLetters = letterstartjson->num;
	float* points = (float*)calloc(pointsLen, sizeof(float));
	short* form = (short*)calloc(formLen, sizeof(short));

	for(int idx = 0; idx < pointsLen; idx++){
		points[idx] = (float)((*pointsjson)[idx].getDouble());
	}
	for(int idx = 0; idx < formLen; idx++){
		form[idx] = (*formjson)[idx].getInt();
	}
	myfont.letterStart = new short[numLetters];
	myfont.letterLen = new short[numLetters];
	for(int idx = 0; idx < numLetters; idx++){
		myfont.letterStart[idx] = (*letterstartjson)[idx].getInt();
		if(idx != 0){
			myfont.letterLen[idx-1] = (myfont.letterStart[idx]-myfont.letterStart[idx-1]);
		}
	}
	myfont.letterLen[numLetters - 1] = (formLen-myfont.letterStart[numLetters - 1]);
	
	glGenBuffers(1, &(myfont.vertex_buffer));
	glBindBuffer(GL_ARRAY_BUFFER, myfont.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*pointsLen, points, GL_STATIC_DRAW);

	glGenBuffers(1, &(myfont.ref_buffer));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, myfont.ref_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(short)*formLen, form, GL_STATIC_DRAW);
	
	free(form);
	free(points);
	fclose(fontfp);
	fontjson->destroy();
	free(fontjson);
}

void destroyFont() {
	delete[] myfont.letterStart;
	delete[] myfont.letterLen;
}
