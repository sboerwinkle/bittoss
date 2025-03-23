#include "../builderContext.h"

void mkRefuelIsland(builderContext * _ctx) {
	builderContext &ctx = *_ctx;
	ctx.center(6400, 0, 0);
	ctx.radius(3200, 3200, 500);
	ctx.bp.typeMask = 19;
	ctx.bp.color = 0xCCCCFF;
	ctx.bp.color = 13421823;
	ctx.build();
	ctx.center(0, -6400, 0);
	ctx.build();
	ctx.center(0, 6400, 0);
	ctx.build();
	ctx.center(-6400, 0, 0);
	ctx.build();
	ctx.center(6400, -6400, 0);
	ctx.bp.color = 0xFFFFFF;
	ctx.bp.color = 16777215;
	ctx.build();
	ctx.center(6400, 6400, 0);
	ctx.build();
	ctx.center(0, 0, 0);
	ctx.build();
	ctx.center(-6400, -6400, 0);
	ctx.build();
	ctx.center(-6400, 6400, 0);
	ctx.build();
	ctx.finish();
}
