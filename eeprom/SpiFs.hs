{-# LANGUAGE DeriveGeneric #-}
{-# LANGUAGE OverloadedStrings #-}

module SpiFs where

import           Control.Applicative
import qualified Crypto.Hash.SHA256 as SHA256
import           Data.Binary
import           Data.Binary.Get
import qualified Data.ByteString as BS
import qualified Data.Text as T
import qualified Data.Text.Encoding as T
import           Data.Word
import           GHC.Generics

data FirmwareFile = FirmwareFile
  { firmwareEntries :: [ Entry ]
  } deriving (Generic, Show)

instance Binary FirmwareFile where
  get = FirmwareFile <$> many readEntry
  put = undefined

data Entry = Entry
  { entryOffset :: ByteOffset
  , entryMagic :: Word32
  , entrySize :: Word32
  , entryBody :: BS.ByteString
  , entryPadding :: ByteOffset
  } | FileEntry
  { entryFileOffset :: ByteOffset
  , entryFileMagic :: Word32
  , entryFileName :: T.Text
  , entryFileBody :: BS.ByteString
  , entryFileHashes :: Maybe (BS.ByteString, BS.ByteString)
  } | EntryPadding deriving Show

data LayoutFile = LayoutFile
  { lfName :: Maybe T.Text
  , lfMagic :: Word32
  , lfPosition :: ByteOffset
  } deriving (Show, Generic)

readEntry :: Get Entry
readEntry = do
  offset <- bytesRead
  magic <- getWord32be
  len <- getWord32be
  body <- getByteString $ fromIntegral len
  tailOffset <- bytesRead
  let
    remainder = tailOffset `mod` 8
    paddingLen = if remainder == 0 then 0 else (8 - remainder)
    handlePlainFile :: Entry
    handlePlainFile = do
      let
        filename :: T.Text
        filename = T.decodeUtf8 $ BS.takeWhile (\c -> c /= 0) $ BS.take 16 body
        filebody = BS.drop 16 body
      FileEntry offset magic filename filebody Nothing
    handleHashedFile = do
      let
        (part1, middle) = BS.splitAt 16 body
        (filebody, hash) = BS.splitAt ((BS.length middle) - 32) middle
        filename :: T.Text
        filename = T.decodeUtf8 $ BS.takeWhile (\c -> c /= 0) part1
        actualHash = SHA256.hash (filebody)
      FileEntry offset magic filename (filebody <> hash) (Just (hash, actualHash))
    handleFileMagic :: Word32 -> Entry
    handleFileMagic 0x55aaf00f = FileEntry offset magic "bootcode.bin" body Nothing
    handleFileMagic 0x55aaf11f = handlePlainFile
    handleFileMagic 0x55aaf22f = handlePlainFile
    handleFileMagic 0x55aaf33f = handleHashedFile
    handleFileMagic 0x55aafeef = EntryPadding
    handleFileMagic _ = Entry offset magic len body tailOffset
  _padding <- getByteString $ fromIntegral paddingLen
  pure $ handleFileMagic magic

generateLayoutFile :: FirmwareFile -> [LayoutFile]
generateLayoutFile = (map entryToLayoutFile) . (filter isPadding) . firmwareEntries
  where
    isPadding EntryPadding = True
    isPadding _ = False
    entryToLayoutFile :: Entry -> LayoutFile
    entryToLayoutFile (FileEntry position magic name _ _) = LayoutFile (Just name) magic position
    entryToLayoutFile (Entry position magic _ _ _) = LayoutFile Nothing magic position
    entryToLayoutFile (EntryPadding) = LayoutFile Nothing 0x55aaf33f 0
