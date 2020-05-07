{-# LANGUAGE DeriveGeneric #-}
{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE NamedFieldPuns #-}

module Main where

import Data.Binary
import Data.Binary.Get
import qualified Data.ByteString as BS
import GHC.Generics
import Control.Applicative
import System.Environment
import Formatting ((%), sformat, hex, shown, stext)
import qualified Data.Text as T
import qualified Data.Text.Encoding as T

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
  { entryFileMagic :: Word32
  , entryFileName :: T.Text
  , entryFileBody :: BS.ByteString
  } deriving Show

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
  padding <- getByteString $ fromIntegral paddingLen
  if ( (magic == 0x55aaf22f) || (magic == 0x55aaf11f) || (magic == 0x55aaf33f) )
    then do
      let
        filename :: T.Text
        filename = T.decodeUtf8 $ BS.takeWhile (\c -> c /= 0) $ BS.take 16 body
        filebody = BS.drop 16 body
      pure $ FileEntry magic filename filebody
    else
      pure $ Entry offset magic len body tailOffset

decodeFirmware :: FilePath -> IO FirmwareFile
decodeFirmware = decodeFile

main :: IO ()
main = do
  args <- getArgs
  let
    prettyPrintEntry :: Entry -> IO ()
    prettyPrintEntry Entry{entryMagic, entryBody} = do
      let
        formatter = "Entry magic=" % hex % " initial_body=" % shown
      putStrLn $ T.unpack $ sformat formatter entryMagic (BS.take 32 entryBody)
    prettyPrintEntry FileEntry{entryFileMagic, entryFileName, entryFileBody} = do
      let
        formatter1 = "File Entry magic=" % hex % " name=" % stext % " body=" % shown
        formatter2 = "File Entry magic=" % hex % " name=" % stext % " body=hidden"
        go2 = putStrLn $ T.unpack $ sformat formatter1 entryFileMagic entryFileName (BS.take 100 entryFileBody)
        go3 = putStrLn $ T.unpack $ sformat formatter2 entryFileMagic entryFileName
      case entryFileName of
        "bootconf.txt" -> go2
        _ -> go3
    go :: [String] -> IO ()
    go [file] = do
      parsed <- decodeFirmware file
      mapM_ prettyPrintEntry (firmwareEntries parsed)
      mapM_ extractFiles (firmwareEntries parsed)
  go args

extractFiles :: Entry -> IO ()
extractFiles Entry{entryMagic, entryBody}
  | entryMagic == 0x55aaf00f = do
    BS.writeFile "bootcode.bin" entryBody
  | otherwise = pure ()
extractFiles FileEntry{entryFileName, entryFileBody} = do
  BS.writeFile (T.unpack entryFileName) entryFileBody
