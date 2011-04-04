/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef DisplayModel_h
#define DisplayModel_h

#include "PdfEngine.h"
#include "DisplayState.h"
#include "PdfSearch.h"

#ifndef USER_DEFAULT_SCREEN_DPI
// the following is only defined if _WIN32_WINNT >= 0x0600 and we use 0x0500
#define USER_DEFAULT_SCREEN_DPI 96
#endif

// define the following if you want shadows drawn around the pages
// #define DRAW_PAGE_SHADOWS

class WindowInfo;

// TODO: better name, Paddings? PaddingInfo?
typedef struct DisplaySettings {
    int     pageBorderTop;
    int     pageBorderBottom;
    int     pageBorderLeft;
    int     pageBorderRight;
    int     betweenPagesX;
    int     betweenPagesY;
} DisplaySettings;

extern DisplaySettings gDisplaySettings;
extern DisplaySettings gDisplaySettingsPresentation;

/* the default distance between a page and window border edges, in pixels */
#ifdef DRAW_PAGE_SHADOWS
#define PADDING_PAGE_BORDER_TOP_DEF      5
#define PADDING_PAGE_BORDER_BOTTOM_DEF   7
#define PADDING_PAGE_BORDER_LEFT_DEF     5
#define PADDING_PAGE_BORDER_RIGHT_DEF    7
/* the distance between pages in y axis, in pixels. Only applicable if
   more than one page in y axis (continuous mode) */
#define PADDING_BETWEEN_PAGES_Y_DEF      8
#else
// TODO: those should probably be more modular so that we can control space
// before first page, after last page and between pages (currently
// betwen pages = first page + last page (i.e. top + bottom)
#define PADDING_PAGE_BORDER_TOP_DEF      2
#define PADDING_PAGE_BORDER_BOTTOM_DEF   2
#define PADDING_PAGE_BORDER_LEFT_DEF     4
#define PADDING_PAGE_BORDER_RIGHT_DEF    4
/* the distance between pages in y axis, in pixels. Only applicable if
   more than one page in y axis (continuous mode) */
#define PADDING_BETWEEN_PAGES_Y_DEF      PADDING_PAGE_BORDER_TOP_DEF + PADDING_PAGE_BORDER_BOTTOM_DEF
#endif
/* the distance between pages in x axis, in pixels. Only applicable if
   columns > 1 */
#define PADDING_BETWEEN_PAGES_X_DEF      PADDING_BETWEEN_PAGES_Y_DEF

#define POINT_OUT_OF_PAGE           0

#define NAV_HISTORY_LEN             50

/* Describes many attributes of one page in one, convenient place */
typedef struct PdfPageInfo {
    /* data that is constant for a given page. page size and rotation
       recorded in PDF file */
    SizeD           page;
    int             rotation;

    /* data that is calculated when needed. actual content size within a page (View target) */
    RectI           contentBox;

    /* data that needs to be set before DisplayModel::relayout().
       Determines whether a given page should be shown on the screen. */
    bool            shown;

    /* data that changes when zoom and rotation changes */
    /* position and size within total area after applying zoom and rotation.
       Represents display rectangle for a given page.
       Calculated in DisplayModel::relayout() */
    RectI           currPos;
    /* data that changes due to scrolling. Calculated in DisplayModel::recalcVisibleParts() */
    float           visibleRatio; /* (0.0 = invisible, 1.0 = fully visible) */
    /* part of the image that should be shown */
    RectI           bitmap;
    /* where it should be blitted on the screen */
    int             screenX, screenY;
    /* position of page relative to visible draw area */
    RectI           pageOnScreen;
} PdfPageInfo;

/* Information needed to drive the display of a given PDF document on a screen.
   You can think of it as a model in the MVC pardigm.
   All the display changes should be done through changing this model via
   API and re-displaying things based on new display information */
class DisplayModel
{
public:
    DisplayModel(DisplayMode displayMode, int dpi=USER_DEFAULT_SCREEN_DPI);
    ~DisplayModel();

    RenderedBitmap *renderBitmap(int pageNo, float zoom, int rotation,
                         RectD *pageRect=NULL, /* if NULL: defaults to the page's mediabox */
                         RenderTarget target=Target_View, bool useGdi=false) {
        if (!engine) return NULL;
        return engine->RenderBitmap(pageNo, zoom, rotation, pageRect, target, useGdi);
    }
    bool renderPage(HDC hDC, int pageNo, RectI *screenRect, float zoom=0, int rotation=0, RectD *pageRect=NULL, RenderTarget target=Target_View) {
        if (!engine) return false;
        return engine->RenderPage(hDC, pageNo, screenRect, zoom, rotation, pageRect, target);
    }

    /* number of pages in PDF document */
    int  pageCount() const { return engine->PageCount(); }
    bool validPageNo(int pageNo) const {
        return 1 <= pageNo && pageNo <= engine->PageCount();
    }

    bool hasTocTree() {
        if (!pdfEngine)
            return false;
        return pdfEngine->hasTocTree();
    }
    PdfTocItem *getTocTree() {
        if (!pdfEngine)
            return NULL;
        return pdfEngine->getTocTree();
    }

    /* current rotation selected by user */
    int rotation() const { return _rotation; }
    void setRotation(int rotation) { _rotation = rotation; }

    DisplayMode displayMode() const { return _displayMode; }
    void changeDisplayMode(DisplayMode displayMode);
    void setPresentationMode(bool enable);
    bool getPresentationMode() const { return _presentationMode; }

    const TCHAR *fileName() const { return engine->FileName(); }

    /* a "virtual" zoom level. Can be either a real zoom level in percent
       (i.e. 100.0 is original size) or one of virtual values ZOOM_FIT_PAGE,
       ZOOM_FIT_WIDTH or ZOOM_FIT_CONTENT, whose real value depends on draw area size */
    float zoomVirtual() const { return _zoomVirtual; }

    float zoomReal() const { return _zoomReal; }
    float zoomReal(int pageNo);

    int startPage() const { return _startPage; }

    int currentPageNo() const;

    BaseEngine *    engine;
    PdfEngine *     pdfEngine;
    PdfSelection *  textSelection;

    /* TODO: rename to pageInfo() */
    PdfPageInfo * getPageInfo(int pageNo) const;

    /* an array of PdfPageInfo, len of array is pageCount */
    PdfPageInfo *   _pagesInfo;

    /* areaOffset is "polymorphic". If drawAreaSize.dx > totalAreSize.dx then
       areaOffset.x is offset of total area rect inside draw area, otherwise
       an offset of draw area inside total area.
       The same for areaOff.y, except it's for dy */
    PointI          areaOffset;

    /* size of draw area (excluding scrollbars) */
    SizeI           drawAreaSize;

    void            setTotalDrawAreaSize(SizeI size) { drawAreaSize = size; }
    
    bool            needHScroll() { return drawAreaSize.dx < _canvasSize.dx; }
    bool            needVScroll() { return drawAreaSize.dy < _canvasSize.dy; }

    void            changeTotalDrawAreaSize(SizeI totalDrawAreaSize);

    bool            pageShown(int pageNo);
    bool            pageVisible(int pageNo);
    bool            pageVisibleNearby(int pageNo);
    int             firstVisiblePageNo() const;
    bool            firstBookPageVisible();
    bool            lastBookPageVisible();
    void            relayout(float zoomVirtual, int rotation);

    void            goToPage(int pageNo, int scrollY, bool addNavPt=false, int scrollX=-1);
    bool            goToPrevPage(int scrollY);
    bool            goToNextPage(int scrollY);
    bool            goToFirstPage();
    bool            goToLastPage();

    void            scrollXTo(int xOff);
    void            scrollXBy(int dx);

    void            scrollYTo(int yOff);
    void            scrollYBy(int dy, bool changePage);

    void            zoomTo(float zoomVirtual, PointI *fixPt=NULL);
    void            zoomBy(float zoomFactor, PointI *fixPt=NULL);
    void            rotateBy(int rotation);

    TCHAR *         getTextInRegion(int pageNo, RectD *region);
    TCHAR *         extractAllText(RenderTarget target=Target_View);

    pdf_link *      getLinkAtPosition(PointI pt);
    int             getPdfLinks(int pageNo, pdf_link **links);
    TCHAR *         getLinkPath(pdf_link *link);
    void            goToTocLink(pdf_link *link);
    void            goToNamedDest(const char *name);
    bool            isOverText(int x, int y);
    pdf_annot *     getCommentAtPosition(PointI pt);

    bool            cvtUserToScreen(int pageNo, PointD *pt);
    bool            cvtScreenToUser(int *pageNo, PointD *pt);
    bool            rectCvtUserToScreen(int pageNo, RectD *r);
    bool            rectCvtScreenToUser(int *pageNo, RectD *r);
    RectD           getContentBox(int pageNo, RenderTarget target=Target_View);

    void            SetFindMatchCase(bool match) { _pdfSearch->SetSensitive(match); }
    PdfSel *        Find(PdfSearchDirection direction=FIND_FORWARD, TCHAR *text=NULL, UINT fromPage=0);
    // note: lastFoundPage might not be a valid page number!
    int             lastFoundPage() const { return _pdfSearch->findPage; }
    bool            bFoundText;

    int             getPageNoByPoint(PointI pt);

    bool            ShowResultRectToScreen(PdfSel *res);

    bool            getScrollState(ScrollState *state);
    void            setScrollState(ScrollState *state);

    bool            addNavPoint(bool keepForward=false);
    bool            canNavigate(int dir) const;
    void            navigate(int dir);

    bool            saveStreamAs(unsigned char *data, int dataLen, const TCHAR *fileName);

    bool            displayStateFromModel(DisplayState *ds);

    void            ageStore() const {
        if (pdfEngine)
            pdfEngine->ageStore();
    }

    void            StartRenderingPage(int pageNo);

protected:

    bool            load(const TCHAR *fileName, int startPage, WindowInfo *win);

    bool            buildPagesInfo();
    float           zoomRealFromVirtualForPage(float zoomVirtual, int pageNo);
    void            changeStartPage(int startPage);
    void            getContentStart(int pageNo, int *x, int *y);
    void            setZoomVirtual(float zoomVirtual);
    void            recalcVisibleParts();
    void            renderVisibleParts();
    /* Those need to be implemented somewhere else by the GUI */
    void            setScrollbarsState();
    /* called when a page number changes */
    void            pageChanged();
    /* called when this DisplayModel is destroyed */
    void            clearAllRenderings();
public:
    /* called when we decide that the display needs to be redrawn */
    void            RepaintDisplay();

protected:
    void            goToPdfDest(fz_obj *dest);

    PdfSearch *     _pdfSearch;
    DisplayMode     _displayMode;
    /* In non-continuous mode is the first page from a PDF file that we're
       displaying.
       No meaning in continous mode. */
    int             _startPage;

    /* an arbitrary pointer that can be used by an app e.g. a multi-window GUI
       could link this to a data describing window displaying  this document */
    // TODO: code depends on this being a WindowInfo in several places
    WindowInfo *    _appData;

    /* size of virtual canvas containing all rendered pages. */
    SizeI           _canvasSize;
    DisplaySettings * _padding;

    /* real zoom value calculated from zoomVirtual. Same as zoomVirtual * 0.01 except
       for ZOOM_FIT_PAGE, ZOOM_FIT_WIDTH and ZOOM_FIT_CONTENT */
    float           _zoomReal;
    float           _zoomVirtual;
    int             _rotation;
    /* dpi correction factor by which _zoomVirtual has to be multiplied in
       order to get _zoomReal */
    float           _dpiFactor;
    /* whether to display pages Left-to-Right or Right-to-Left.
       this value is extracted from the PDF document */
    bool            _displayR2L;

    /* when we're in presentation mode, _pres* contains the pre-presentation values */
    bool            _presentationMode;
    float           _presZoomVirtual;
    DisplayMode     _presDisplayMode;

    ScrollState   * _navHistory;
    int             _navHistoryIx;
    int             _navHistoryEnd;

public:
    /* allow resizing a window without triggering a new rendering (needed for window destruction) */
    bool            _dontRenderFlag;

    static DisplayModel *CreateFromFileName(WindowInfo *win, const TCHAR *fileName,
                                            DisplayMode displayMode, int startPage);
};

bool    displayModeContinuous(DisplayMode displayMode);
bool    displayModeFacing(DisplayMode displayMode);
bool    displayModeShowCover(DisplayMode displayMode);
int     columnsFromDisplayMode(DisplayMode displayMode);
bool    rotationFlipped(int rotation);

#endif
