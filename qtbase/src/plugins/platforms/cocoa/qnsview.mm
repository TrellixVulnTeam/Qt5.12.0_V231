/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtGui/qtguiglobal.h>

#include "qnsview.h"
#include "qcocoawindow.h"
#include "qcocoahelpers.h"
#include "qcocoascreen.h"
#include "qmultitouch_mac_p.h"
#include "qcocoadrag.h"
#include "qcocoainputcontext.h"
#include <qpa/qplatformintegration.h>

#include <qpa/qwindowsysteminterface.h>
#include <QtGui/QTextFormat>
#include <QtCore/QDebug>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/qsysinfo.h>
#include <QtGui/QAccessible>
#include <QtGui/QImage>
#include <private/qguiapplication_p.h>
#include <private/qcoregraphics_p.h>
#include <private/qwindow_p.h>
#include "qcocoabackingstore.h"
#ifndef QT_NO_OPENGL
#include "qcocoaglcontext.h"
#endif
#include "qcocoaintegration.h"

// Private interface
@interface QT_MANGLE_NAMESPACE(QNSView) ()
- (BOOL)isTransparentForUserInput;
@end

@interface QT_MANGLE_NAMESPACE(QNSView) (Drawing) <CALayerDelegate>
- (void)initDrawing;
@end

@interface QT_MANGLE_NAMESPACE(QNSViewMouseMoveHelper) : NSObject
- (instancetype)initWithView:(QNSView *)theView;
- (void)mouseMoved:(NSEvent *)theEvent;
- (void)mouseEntered:(NSEvent *)theEvent;
- (void)mouseExited:(NSEvent *)theEvent;
- (void)cursorUpdate:(NSEvent *)theEvent;
@end

@interface QT_MANGLE_NAMESPACE(QNSView) (Mouse)
- (NSPoint)screenMousePoint:(NSEvent *)theEvent;
- (void)mouseMovedImpl:(NSEvent *)theEvent;
- (void)mouseEnteredImpl:(NSEvent *)theEvent;
- (void)mouseExitedImpl:(NSEvent *)theEvent;
@end

@interface QT_MANGLE_NAMESPACE(QNSView) (Touch)
@end

@interface QT_MANGLE_NAMESPACE(QNSView) (Tablet)
- (bool)handleTabletEvent:(NSEvent *)theEvent;
@end

@interface QT_MANGLE_NAMESPACE(QNSView) (Gestures)
@end

@interface QT_MANGLE_NAMESPACE(QNSView) (Dragging)
-(void)registerDragTypes;
@end

@interface QT_MANGLE_NAMESPACE(QNSView) (Keys)
@end

@interface QT_MANGLE_NAMESPACE(QNSView) (ComplexText) <NSTextInputClient>
- (void)textInputContextKeyboardSelectionDidChangeNotification:(NSNotification *)textInputContextKeyboardSelectionDidChangeNotification;
@end

@implementation QT_MANGLE_NAMESPACE(QNSView) {
    QPointer<QCocoaWindow> m_platformWindow;
    NSTrackingArea *m_trackingArea;
    Qt::MouseButtons m_buttons;
    Qt::MouseButtons m_acceptedMouseDowns;
    Qt::MouseButtons m_frameStrutButtons;
    QString m_composingText;
    QPointer<QObject> m_composingFocusObject;
    bool m_sendKeyEvent;
    bool m_dontOverrideCtrlLMB;
    bool m_sendUpAsRightButton;
    Qt::KeyboardModifiers m_currentWheelModifiers;
    NSString *m_inputSource;
    QT_MANGLE_NAMESPACE(QNSViewMouseMoveHelper) *m_mouseMoveHelper;
    bool m_resendKeyEvent;
    bool m_scrolling;
    bool m_updatingDrag;
    NSEvent *m_currentlyInterpretedKeyEvent;
    QSet<quint32> m_acceptedKeyDowns;
}

- (instancetype)initWithCocoaWindow:(QCocoaWindow *)platformWindow
{
    if ((self = [super initWithFrame:NSZeroRect])) {
        m_platformWindow = platformWindow;
        m_buttons = Qt::NoButton;
        m_acceptedMouseDowns = Qt::NoButton;
        m_frameStrutButtons = Qt::NoButton;
        m_sendKeyEvent = false;
        m_sendUpAsRightButton = false;
        m_inputSource = nil;
        m_mouseMoveHelper = [[QT_MANGLE_NAMESPACE(QNSViewMouseMoveHelper) alloc] initWithView:self];
        m_resendKeyEvent = false;
        m_scrolling = false;
        m_updatingDrag = false;
        m_currentlyInterpretedKeyEvent = nil;
        m_dontOverrideCtrlLMB = qt_mac_resolveOption(false, platformWindow->window(),
            "_q_platform_MacDontOverrideCtrlLMB", "QT_MAC_DONT_OVERRIDE_CTRL_LMB");
        m_trackingArea = nil;

        self.focusRingType = NSFocusRingTypeNone;
        self.cursor = nil;

        [self initDrawing];
        [self registerDragTypes];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                              selector:@selector(textInputContextKeyboardSelectionDidChangeNotification:)
                                              name:NSTextInputContextKeyboardSelectionDidChangeNotification
                                              object:nil];
    }
    return self;
}

- (void)dealloc
{
    if (m_trackingArea) {
        [self removeTrackingArea:m_trackingArea];
        [m_trackingArea release];
    }
    [m_inputSource release];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [m_mouseMoveHelper release];

    [super dealloc];
}

- (NSString *)description
{
    NSMutableString *description = [NSMutableString stringWithString:[super description]];

#ifndef QT_NO_DEBUG_STREAM
    QString platformWindowDescription;
    QDebug debug(&platformWindowDescription);
    debug.nospace() << "; " << m_platformWindow << ">";

    NSRange lastCharacter = [description rangeOfComposedCharacterSequenceAtIndex:description.length - 1];
    [description replaceCharactersInRange:lastCharacter withString:platformWindowDescription.toNSString()];
#endif

    return description;
}

- (void)viewDidMoveToSuperview
{
    if (!m_platformWindow)
        return;

    if (!m_platformWindow->isEmbedded())
        return;

    if ([self superview]) {
        QWindowSystemInterface::handleGeometryChange(m_platformWindow->window(), m_platformWindow->geometry());
        [self setNeedsDisplay:YES];
        QWindowSystemInterface::flushWindowSystemEvents();
    }
}

- (QWindow *)topLevelWindow
{
    if (!m_platformWindow)
        return nullptr;

    QWindow *focusWindow = m_platformWindow->window();

    // For widgets we need to do a bit of trickery as the window
    // to activate is the window of the top-level widget.
    if (qstrcmp(focusWindow->metaObject()->className(), "QWidgetWindow") == 0) {
        while (focusWindow->parent()) {
            focusWindow = focusWindow->parent();
        }
    }

    return focusWindow;
}

- (void)viewDidHide
{
    if (!m_platformWindow->isExposed())
        return;

    m_platformWindow->handleExposeEvent(QRegion());

    // Note: setNeedsDisplay is automatically called for
    // viewDidUnhide so no reason to override it here.
}

- (void)removeFromSuperview
{
    QMacAutoReleasePool pool;
    [super removeFromSuperview];
}

- (BOOL)isTransparentForUserInput
{
    return m_platformWindow->window() &&
        m_platformWindow->window()->flags() & Qt::WindowTransparentForInput;
}

- (BOOL)becomeFirstResponder
{
    if (!m_platformWindow)
        return NO;
    if ([self isTransparentForUserInput])
        return NO;
    if (!m_platformWindow->windowIsPopupType())
        QWindowSystemInterface::handleWindowActivated([self topLevelWindow]);
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    if (!m_platformWindow)
        return NO;
    if (m_platformWindow->shouldRefuseKeyWindowAndFirstResponder())
        return NO;
    if ([self isTransparentForUserInput])
        return NO;
    if ((m_platformWindow->window()->flags() & Qt::ToolTip) == Qt::ToolTip)
        return NO;
    return YES;
}

- (NSView *)hitTest:(NSPoint)aPoint
{
    NSView *candidate = [super hitTest:aPoint];
    if (candidate == self) {
        if ([self isTransparentForUserInput])
            return nil;
    }
    return candidate;
}

- (void)convertFromScreen:(NSPoint)mouseLocation toWindowPoint:(QPointF *)qtWindowPoint andScreenPoint:(QPointF *)qtScreenPoint
{
    // Calculate the mouse position in the QWindow and Qt screen coordinate system,
    // starting from coordinates in the NSWindow coordinate system.
    //
    // This involves translating according to the window location on screen,
    // as well as inverting the y coordinate due to the origin change.
    //
    // Coordinate system overview, outer to innermost:
    //
    // Name             Origin
    //
    // OS X screen      bottom-left
    // Qt screen        top-left
    // NSWindow         bottom-left
    // NSView/QWindow   top-left
    //
    // NSView and QWindow are equal coordinate systems: the QWindow covers the
    // entire NSView, and we've set the NSView's isFlipped property to true.

    NSWindow *window = [self window];
    NSPoint nsWindowPoint;
    NSRect windowRect = [window convertRectFromScreen:NSMakeRect(mouseLocation.x, mouseLocation.y, 1, 1)];
    nsWindowPoint = windowRect.origin;                    // NSWindow coordinates
    NSPoint nsViewPoint = [self convertPoint: nsWindowPoint fromView: nil]; // NSView/QWindow coordinates
    *qtWindowPoint = QPointF(nsViewPoint.x, nsViewPoint.y);                     // NSView/QWindow coordinates
    *qtScreenPoint = QCocoaScreen::mapFromNative(mouseLocation);
}

@end

#include "qnsview_drawing.mm"
#include "qnsview_mouse.mm"
#include "qnsview_touch.mm"
#include "qnsview_gestures.mm"
#include "qnsview_tablet.mm"
#include "qnsview_dragging.mm"
#include "qnsview_keys.mm"
#include "qnsview_complextext.mm"
#include "qnsview_menus.mm"
#ifndef QT_NO_ACCESSIBILITY
#include "qnsview_accessibility.mm"
#endif

// -----------------------------------------------------

@implementation QT_MANGLE_NAMESPACE(QNSView) (QtExtras)

- (QCocoaWindow*)platformWindow
{
    return m_platformWindow.data();;
}

@end
