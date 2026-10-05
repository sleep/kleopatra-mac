/*
    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2026 Kleopatra contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "macosstyle.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QHash>
#include <QIconEngine>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>

#import <AppKit/AppKit.h>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace
{
bool nativeStyleActive = true;

// Draws an SF Symbol as a template image tinted with the palette color for the requested
// mode. The tinting happens when painting, so that the icon follows appearance changes.
// SF Symbols belong to the look of macOS; with other widget styles the fallback icon is
// drawn instead. This is also decided when painting, so that the icon follows style changes.
class SymbolIconEngine : public QIconEngine
{
public:
    explicit SymbolIconEngine(const QString &symbolName, const QIcon &fallback)
        : mSymbolName{symbolName}
        , mFallback{fallback}
    {
    }

    QIconEngine *clone() const override
    {
        return new SymbolIconEngine{mSymbolName, mFallback};
    }

    QString key() const override
    {
        return u"KleopatraSymbolIconEngine"_s;
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        if (useFallback()) {
            mFallback.paint(painter, rect, Qt::AlignCenter, mode, state);
            return;
        }
        const qreal scale = painter->device() ? painter->device()->devicePixelRatioF() : qApp->devicePixelRatio();
        painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, scale));
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        if (useFallback()) {
            return mFallback.pixmap(size, scale, mode, state);
        }
        const QSize pixelSize = size * scale;
        if (pixelSize.isEmpty()) {
            return {};
        }
        QImage image{pixelSize, QImage::Format_ARGB32_Premultiplied};
        image.fill(Qt::transparent);
        @autoreleasepool {
            NSImage *symbol = [NSImage imageWithSystemSymbolName:mSymbolName.toNSString() accessibilityDescription:nil];
            if (!symbol) {
                return {};
            }
            // Symbols are drawn smaller than the icon size, like in native toolbars. With this
            // point size also the wider ones of the symbols in use fit into a square icon, so
            // that all symbols are drawn with the same size and stroke width.
            NSImageSymbolConfiguration *config = [NSImageSymbolConfiguration configurationWithPointSize:size.height() * 0.62
                                                                                                 weight:NSFontWeightRegular
                                                                                                  scale:NSImageSymbolScaleMedium];
            symbol = [symbol imageWithSymbolConfiguration:config];
            CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
            CGContextRef context = CGBitmapContextCreate(image.bits(),
                                                         image.width(),
                                                         image.height(),
                                                         8,
                                                         image.bytesPerLine(),
                                                         colorSpace,
                                                         kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
            CGColorSpaceRelease(colorSpace);
            if (!context) {
                return {};
            }
            NSGraphicsContext *graphicsContext = [NSGraphicsContext graphicsContextWithCGContext:context flipped:NO];
            [NSGraphicsContext saveGraphicsState];
            [NSGraphicsContext setCurrentContext:graphicsContext];
            NSSize symbolSize = symbol.size;
            const qreal factor = std::min(pixelSize.width() / symbolSize.width, pixelSize.height() / symbolSize.height);
            symbolSize = NSMakeSize(symbolSize.width * std::min<qreal>(factor, scale), symbolSize.height * std::min<qreal>(factor, scale));
            if (factor < scale) {
                // a symbol that had to be shrunk to fit doesn't have a whole number of pixels anymore
                symbolSize = NSMakeSize(std::max(1.0, std::round(symbolSize.width)), std::max(1.0, std::round(symbolSize.height)));
            }
            // the symbol is aligned to whole pixels; otherwise, it would be blurred
            const NSRect target = NSMakeRect(std::floor((pixelSize.width() - symbolSize.width) / 2),
                                             std::floor((pixelSize.height() - symbolSize.height) / 2),
                                             symbolSize.width,
                                             symbolSize.height);
            [symbol drawInRect:target fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1.0];
            [NSGraphicsContext restoreGraphicsState];
            CGContextRelease(context);
        }
        // the symbol is a template image; color it like text
        const QPalette palette = qApp->palette();
        QColor color;
        switch (mode) {
        case QIcon::Disabled:
            color = palette.color(QPalette::Disabled, QPalette::WindowText);
            break;
        case QIcon::Selected:
            color = palette.color(QPalette::Active, QPalette::HighlightedText);
            break;
        default:
            color = palette.color(QPalette::Active, QPalette::WindowText);
        }
        QPainter painter{&image};
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(image.rect(), color);
        painter.end();
        QPixmap result = QPixmap::fromImage(image);
        result.setDevicePixelRatio(scale);
        return result;
    }

    QSize actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return useFallback() ? mFallback.actualSize(size, mode, state) : size;
    }

private:
    bool useFallback() const
    {
        return !nativeStyleActive && !mFallback.isNull();
    }

private:
    QString mSymbolName;
    QIcon mFallback;
};

bool symbolExists(const QString &symbolName)
{
    @autoreleasepool {
        return [NSImage imageWithSystemSymbolName:symbolName.toNSString() accessibilityDescription:nil] != nil;
    }
}
}

void Kleo::MacOS::setNativeStyleActive(bool active)
{
    nativeStyleActive = active;
}

bool Kleo::MacOS::isNativeStyleActive()
{
    return nativeStyleActive;
}

QIcon Kleo::MacOS::symbolIcon(const QString &symbolName, const QIcon &fallback)
{
    if (!symbolExists(symbolName)) {
        return fallback;
    }
    return QIcon{new SymbolIconEngine{symbolName, fallback}};
}

QIcon Kleo::MacOS::symbolIconFor(const QIcon &icon)
{
    // theme icons used in the toolbars of Kleopatra and their SF Symbol counterparts
    static const QHash<QString, QString> symbols = {
        {u"document-edit-sign-encrypt"_s, u"lock.doc"_s},
        {u"document-edit-decrypt-verify"_s, u"lock.open"_s},
        {u"view-certificate-import"_s, u"square.and.arrow.down"_s},
        {u"view-certificate-export"_s, u"square.and.arrow.up"_s},
        {u"view-certificate-sign"_s, u"checkmark.seal"_s},
        {u"edit-find"_s, u"globe"_s},
        {u"view-certificate"_s, u"list.bullet.rectangle"_s},
        {u"note"_s, u"note.text"_s},
        {u"auth-sim-locked"_s, u"creditcard"_s},
        {u"group"_s, u"person.2"_s},
    };
    const QString symbolName = symbols.value(icon.name());
    return symbolName.isEmpty() ? icon : symbolIcon(symbolName, icon);
}

void Kleo::MacOS::styleItemView(QAbstractItemView *view)
{
    view->setAlternatingRowColors(true);
    view->setFrameShape(QFrame::NoFrame);
}
