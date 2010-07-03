#ifndef __GTK_COMPAT_H__
#define __GTK_COMPAT_H__

#include <gtk/gtk.h>

/* Provide a GTK+ compatibility layer. */

#if GTK_CHECK_VERSION (2,90,5)

/* Recreate GdkRegion until we drop GTK2 compatibility. */

#define GdkRegion cairo_region_t

#define gdk_region_new()		cairo_region_create()
#define gdk_region_destroy(region)	cairo_region_destroy (region)

#define gdk_region_union_with_rect(region, rect) \
	G_STMT_START { \
		if ((rect)->width > 0 && (rect)->height > 0) \
			cairo_region_union_rectangle ((region), (rect)); \
	} G_STMT_END

#endif

#endif /* __GTK_COMPAT_H__ */
