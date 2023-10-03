#include <strings.h>
#include <stdint.h>

#include "list.h"

// The X11 named colors, so Paul will be happy
struct namedColor {
	const char *name;
	int32_t value;
	bool operator<=(const namedColor &other) const;
};

bool namedColor::operator<=(const namedColor &other) const {
	return strcasecmp(name, other.name) <= 0;
}

static list<namedColor> colors;

int32_t findColor(const char* name) {
	int ix = colors.s_find({.name=name});
	if (ix < 0) return -2;
	return colors[ix].value;
}

void colors_destroy() {
	colors.destroy();
}

void colors_init() {
	colors.init(157);
	colors.add({.name="1.1", .value=0x00c000}); // Default colors, by team (green vs red)
	colors.add({.name="1.2", .value=0xa0c000});
	colors.add({.name="1.3", .value=0x00a080});
	colors.add({.name="2.1", .value=0xc00000});
	colors.add({.name="2.2", .value=0xc06000});
	colors.add({.name="2.3", .value=0xa00060});
	colors.add({.name="aliceblue", .value=0xf0f8ff});
	colors.add({.name="antiquewhite", .value=0xfaebd7});
	colors.add({.name="aqua", .value=0x00ffff});
	colors.add({.name="aquamarine", .value=0x7fffd4});
	colors.add({.name="azure", .value=0xf0ffff});
	colors.add({.name="beige", .value=0xf5f5dc});
	colors.add({.name="bisque", .value=0xffe4c4});
	colors.add({.name="black", .value=0x000000});
	colors.add({.name="blanchedalmond", .value=0xffebcd});
	colors.add({.name="blue", .value=0x0000ff});
	colors.add({.name="blueviolet", .value=0x8a2be2});
	colors.add({.name="brown", .value=0xa52a2a});
	colors.add({.name="burlywood", .value=0xdeb887});
	colors.add({.name="cactus", .value=0x53713d});
	colors.add({.name="cadetblue", .value=0x5f9ea0});
	colors.add({.name="chartreuse", .value=0x7fff00});
	colors.add({.name="chocolate", .value=0xd2691e});
	colors.add({.name="clear", .value=-1});
	colors.add({.name="coral", .value=0xff7f50});
	colors.add({.name="cornflowerblue", .value=0x6495ed});
	colors.add({.name="cornsilk", .value=0xfff8dc});
	colors.add({.name="crimson", .value=0xdc143c});
	colors.add({.name="cyan", .value=0x00ffff});
	colors.add({.name="darkblue", .value=0x00008b});
	colors.add({.name="darkcyan", .value=0x008b8b});
	colors.add({.name="darkgoldenrod", .value=0xb8860b});
	colors.add({.name="darkgray", .value=0xa9a9a9});
	colors.add({.name="darkgreen", .value=0x006400});
	colors.add({.name="darkgrey", .value=0xa9a9a9});
	colors.add({.name="darkkhaki", .value=0xbdb76b});
	colors.add({.name="darkmagenta", .value=0x8b008b});
	colors.add({.name="darkolivegreen", .value=0x556b2f});
	colors.add({.name="darkorange", .value=0xff8c00});
	colors.add({.name="darkorchid", .value=0x9932cc});
	colors.add({.name="darkred", .value=0x8b0000});
	colors.add({.name="darksalmon", .value=0xe9967a});
	colors.add({.name="darkseagreen", .value=0x8fbc8f});
	colors.add({.name="darkslateblue", .value=0x483d8b});
	colors.add({.name="darkslategray", .value=0x2f4f4f});
	colors.add({.name="darkslategrey", .value=0x2f4f4f});
	colors.add({.name="darkturquoise", .value=0x00ced1});
	colors.add({.name="darkviolet", .value=0x9400d3});
	colors.add({.name="deeppink", .value=0xff1493});
	colors.add({.name="deepskyblue", .value=0x00bfff});
	colors.add({.name="dimgray", .value=0x696969});
	colors.add({.name="dimgrey", .value=0x696969});
	colors.add({.name="dodgerblue", .value=0x1e90ff});
	colors.add({.name="firebrick", .value=0xb22222});
	colors.add({.name="floralwhite", .value=0xfffaf0});
	colors.add({.name="forestgreen", .value=0x228b22});
	colors.add({.name="fuchsia", .value=0xff00ff});
	colors.add({.name="gainsboro", .value=0xdcdcdc});
	colors.add({.name="ghostwhite", .value=0xf8f8ff});
	colors.add({.name="gold", .value=0xffd700});
	colors.add({.name="goldenrod", .value=0xdaa520});
	colors.add({.name="gray", .value=0x808080});
	colors.add({.name="green", .value=0x008000});
	colors.add({.name="greenyellow", .value=0xadff2f});
	colors.add({.name="grey", .value=0x808080});
	colors.add({.name="honeydew", .value=0xf0fff0});
	colors.add({.name="hotpink", .value=0xff69b4});
	colors.add({.name="indianred", .value=0xcd5c5c});
	colors.add({.name="indigo", .value=0x4b0082});
	colors.add({.name="ivory", .value=0xfffff0});
	colors.add({.name="khaki", .value=0xf0e68c});
	colors.add({.name="lavender", .value=0xe6e6fa});
	colors.add({.name="lavenderblush", .value=0xfff0f5});
	colors.add({.name="lawngreen", .value=0x7cfc00});
	colors.add({.name="lemonchiffon", .value=0xfffacd});
	colors.add({.name="lightblue", .value=0xadd8e6});
	colors.add({.name="lightcoral", .value=0xf08080});
	colors.add({.name="lightcyan", .value=0xe0ffff});
	colors.add({.name="lightgoldenrodyellow", .value=0xfafad2});
	colors.add({.name="lightgray", .value=0xd3d3d3});
	colors.add({.name="lightgreen", .value=0x90ee90});
	colors.add({.name="lightgrey", .value=0xd3d3d3});
	colors.add({.name="lightpink", .value=0xffb6c1});
	colors.add({.name="lightsalmon", .value=0xffa07a});
	colors.add({.name="lightseagreen", .value=0x20b2aa});
	colors.add({.name="lightskyblue", .value=0x87cefa});
	colors.add({.name="lightslategray", .value=0x778899});
	colors.add({.name="lightslategrey", .value=0x778899});
	colors.add({.name="lightsteelblue", .value=0xb0c4de});
	colors.add({.name="lightyellow", .value=0xffffe0});
	colors.add({.name="lime", .value=0x00ff00});
	colors.add({.name="limegreen", .value=0x32cd32});
	colors.add({.name="linen", .value=0xfaf0e6});
	colors.add({.name="magenta", .value=0xff00ff});
	colors.add({.name="maroon", .value=0x800000});
	colors.add({.name="mediumaquamarine", .value=0x66cdaa});
	colors.add({.name="mediumblue", .value=0x0000cd});
	colors.add({.name="mediumorchid", .value=0xba55d3});
	colors.add({.name="mediumpurple", .value=0x9370db});
	colors.add({.name="mediumseagreen", .value=0x3cb371});
	colors.add({.name="mediumslateblue", .value=0x7b68ee});
	colors.add({.name="mediumspringgreen", .value=0x00fa9a});
	colors.add({.name="mediumturquoise", .value=0x48d1cc});
	colors.add({.name="mediumvioletred", .value=0xc71585});
	colors.add({.name="midnightblue", .value=0x191970});
	colors.add({.name="mintcream", .value=0xf5fffa});
	colors.add({.name="mistyrose", .value=0xffe4e1});
	colors.add({.name="moccasin", .value=0xffe4b5});
	colors.add({.name="navajowhite", .value=0xffdead});
	colors.add({.name="navy", .value=0x000080});
	colors.add({.name="oldlace", .value=0xfdf5e6});
	colors.add({.name="olive", .value=0x808000});
	colors.add({.name="olivedrab", .value=0x6b8e23});
	colors.add({.name="orange", .value=0xffa500});
	colors.add({.name="orangered", .value=0xff4500});
	colors.add({.name="orchid", .value=0xda70d6});
	colors.add({.name="palegoldenrod", .value=0xeee8aa});
	colors.add({.name="palegreen", .value=0x98fb98});
	colors.add({.name="paleturquoise", .value=0xafeeee});
	colors.add({.name="palevioletred", .value=0xdb7093});
	colors.add({.name="papayawhip", .value=0xffefd5});
	colors.add({.name="peachpuff", .value=0xffdab9});
	colors.add({.name="peru", .value=0xcd853f});
	colors.add({.name="pink", .value=0xffc0cb});
	colors.add({.name="plum", .value=0xdda0dd});
	colors.add({.name="powderblue", .value=0xb0e0e6});
	colors.add({.name="purple", .value=0x800080});
	colors.add({.name="rebeccapurple", .value=0x663399});
	colors.add({.name="red", .value=0xff0000});
	colors.add({.name="rosybrown", .value=0xbc8f8f});
	colors.add({.name="royalblue", .value=0x4169e1});
	colors.add({.name="saddlebrown", .value=0x8b4513});
	colors.add({.name="salmon", .value=0xfa8072});
	colors.add({.name="sandybrown", .value=0xf4a460});
	colors.add({.name="seagreen", .value=0x2e8b57});
	colors.add({.name="seashell", .value=0xfff5ee});
	colors.add({.name="sienna", .value=0xa0522d});
	colors.add({.name="silver", .value=0xc0c0c0});
	colors.add({.name="skyblue", .value=0x87ceeb});
	colors.add({.name="slateblue", .value=0x6a5acd});
	colors.add({.name="slategray", .value=0x708090});
	colors.add({.name="slategrey", .value=0x708090});
	colors.add({.name="snow", .value=0xfffafa});
	colors.add({.name="springgreen", .value=0x00ff7f});
	colors.add({.name="steelblue", .value=0x4682b4});
	colors.add({.name="tan", .value=0xd2b48c});
	colors.add({.name="teal", .value=0x008080});
	colors.add({.name="thistle", .value=0xd8bfd8});
	colors.add({.name="timberwolf", .value=0xd9d6cf}); // Add Crayola Timberwolf, specifically for Paul
	colors.add({.name="tomato", .value=0xff6347});
	colors.add({.name="turquoise", .value=0x40e0d0});
	colors.add({.name="violet", .value=0xee82ee});
	colors.add({.name="wheat", .value=0xf5deb3});
	colors.add({.name="white", .value=0xffffff});
	colors.add({.name="whitesmoke", .value=0xf5f5f5});
	colors.add({.name="yellow", .value=0xffff00});
	colors.add({.name="yellowgreen", .value=0x9acd32});
}
